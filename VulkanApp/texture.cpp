#include "texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

Texture::Texture()
{
	usage_count_ = 0;
}

Texture::~Texture()
{

}

void Texture::Init(VulkanDevices* devices, std::string filename)
{
	vk_device_handle_ = devices->GetLogicalDevice();
	texture_name_ = filename;

	int tex_width, tex_height, tex_channels;
	stbi_uc* pixels = stbi_load(filename.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

	image_size_ = tex_width * tex_height * 4;

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	devices->CreateBuffer(image_size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

	void* data;
	vkMapMemory(vk_device_handle_, staging_buffer_memory, 0, image_size_, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(image_size_));
	vkUnmapMemory(vk_device_handle_, staging_buffer_memory);

	stbi_image_free(pixels);

	// reduce sizes of textures that are larger than max texture resolution
	if (tex_width * tex_height > MAX_TEXTURE_RESOLUTION * MAX_TEXTURE_RESOLUTION)
	{
		int new_width, new_height;
		// determine new resolution for texture
		if (tex_height > tex_width)
		{
			new_height = MAX_TEXTURE_RESOLUTION;
			new_width = tex_width * ((float)new_height / (float)tex_height);
		}
		else
		{
			new_width = MAX_TEXTURE_RESOLUTION;
			new_height = tex_height * ((float)new_width / (float)tex_width);
		}

		// create the initial texture and copy in data from the staging buffer
		VkImage initial_image;
		VkDeviceMemory initial_image_memory;

		devices->CreateImage(tex_width, tex_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, initial_image, initial_image_memory);
		devices->TransitionImageLayout(initial_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		devices->CopyBufferToImage(staging_buffer, initial_image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));
		devices->TransitionImageLayout(initial_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// create the final texture
		devices->CreateImage(new_width, new_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image_, texture_image_memory_);
		devices->TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// blit the image from the initial texture
		VkCommandBuffer blit_buffer = devices->BeginSingleTimeCommands();

		VkImageBlit image_blit = {};
		image_blit.srcOffsets[0] = { 0, 0, 0 };
		image_blit.srcOffsets[1] = { tex_width, tex_height, 1 };
		image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit.srcSubresource.baseArrayLayer = 0;
		image_blit.srcSubresource.layerCount = 1;
		image_blit.srcSubresource.mipLevel = 0;

		image_blit.dstOffsets[0] = { 0, 0, 0 };
		image_blit.dstOffsets[1] = { new_width, new_height, 1 };
		image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_blit.dstSubresource.baseArrayLayer = 0;
		image_blit.dstSubresource.layerCount = 1;
		image_blit.dstSubresource.mipLevel = 0;

		vkCmdBlitImage(blit_buffer, initial_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

		// submit the blit command buffer
		devices->EndSingleTimeCommands(blit_buffer);

		// clean up the initial texture now it is no longer needed
		vkDestroyImage(vk_device_handle_, initial_image, nullptr);
		vkFreeMemory(vk_device_handle_, initial_image_memory, nullptr);
	}
	else
	{
		// create the texture and copy in data from the buffer
		devices->CreateImage(tex_width, tex_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image_, texture_image_memory_);
		devices->TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		devices->CopyBufferToImage(staging_buffer, texture_image_, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));
		devices->TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	vkDestroyBuffer(vk_device_handle_, staging_buffer, nullptr);
	vkFreeMemory(vk_device_handle_, staging_buffer_memory, nullptr);

	texture_image_view_ = devices->CreateImageView(texture_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	
	InitSampler(devices);
}

void Texture::Cleanup()
{
	vkDestroyImageView(vk_device_handle_, texture_image_view_, nullptr);
	vkDestroyImage(vk_device_handle_, texture_image_, nullptr);
	vkFreeMemory(vk_device_handle_, texture_image_memory_, nullptr);
	vkDestroySampler(vk_device_handle_, texture_sampler_, nullptr);
}

void Texture::InitSampler(VulkanDevices* devices)
{
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = 16;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(vk_device_handle_, &sampler_info, nullptr, &texture_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void Texture::AddMapType(MapType map_type)
{
	for (MapType type : map_types_)
	{
		if (type == map_type)
		{
			return;
		}
	}

	map_types_.push_back(map_type);
}