#include "render_target.h"

void VulkanRenderTarget::Init(VulkanDevices* devices, VkFormat format, uint32_t width, uint32_t height, uint32_t count, bool depth_enabled)
{
	devices_ = devices;

	render_target_images_.resize(count);
	render_target_image_views_.resize(count);
	render_target_image_memories_.resize(count);
	render_target_format_ = format;

	// create render targets
	for (int i = 0; i < count; i++)
	{
		devices->CreateImage(width, height, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, render_target_images_[i], render_target_image_memories_[i]);
		render_target_image_views_[i] = devices->CreateImageView(render_target_images_[i], format, VK_IMAGE_ASPECT_COLOR_BIT);
		devices->TransitionImageLayout(render_target_images_[i], format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}

	// if depth is enabled create depth buffer
	if (depth_enabled)
	{
		render_target_depth_format_ = devices_->FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		devices_->CreateImage(width, height, render_target_depth_format_, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, render_target_depth_image_, render_target_depth_image_memory_);
		render_target_depth_image_view_ = devices_->CreateImageView(render_target_depth_image_, render_target_depth_format_, VK_IMAGE_ASPECT_DEPTH_BIT);
		devices_->TransitionImageLayout(render_target_depth_image_, render_target_depth_format_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
}

void VulkanRenderTarget::Cleanup()
{
	// clean up render targets
	for (int i = 0; i < render_target_images_.size(); i++)
	{
		vkDestroyImage(devices_->GetLogicalDevice(), render_target_images_[i], nullptr);
		vkDestroyImageView(devices_->GetLogicalDevice(), render_target_image_views_[i], nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), render_target_image_memories_[i], nullptr);
	}

	// clean up depth resources
	vkDestroyImage(devices_->GetLogicalDevice(), render_target_depth_image_, nullptr);
	vkDestroyImageView(devices_->GetLogicalDevice(), render_target_depth_image_view_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), render_target_depth_image_memory_, nullptr);
}

void VulkanRenderTarget::ClearImage(VkClearColorValue clear_color, int index)
{
	if (index >= 0)
	{
		// clear the image
		// transition the buffers to the correct format for clearing
		devices_->TransitionImageLayout(render_target_images_[index], render_target_format_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// clear the buffers
		VkCommandBuffer clear_buffer = devices_->BeginSingleTimeCommands();

		VkImageSubresourceRange image_range = {};
		image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_range.levelCount = 1;
		image_range.layerCount = 1;

		vkCmdClearColorImage(clear_buffer, render_target_images_[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);

		devices_->EndSingleTimeCommands(clear_buffer);

		// transition buffers back to the correct format
		devices_->TransitionImageLayout(render_target_images_[index], render_target_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	else
	{
		for (int i = 0; i < render_target_images_.size(); i++)
		{
			// transition the buffers to the correct format for clearing
			devices_->TransitionImageLayout(render_target_images_[i], render_target_format_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			// clear the buffers
			VkCommandBuffer clear_buffer = devices_->BeginSingleTimeCommands();

			VkImageSubresourceRange image_range = {};
			image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_range.levelCount = 1;
			image_range.layerCount = 1;

			vkCmdClearColorImage(clear_buffer, render_target_images_[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);

			devices_->EndSingleTimeCommands(clear_buffer);

			// transition buffers back to the correct format
			devices_->TransitionImageLayout(render_target_images_[i], render_target_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
	}
}

void VulkanRenderTarget::ClearDepth()
{
	// clear the  depth stencil view
	// transition the buffers to the correct format for clearing
	devices_->TransitionImageLayout(render_target_depth_image_, render_target_depth_format_, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// clear the buffers
	VkCommandBuffer clear_buffer = devices_->BeginSingleTimeCommands();

	VkClearDepthStencilValue clear_depth = { 1.0f, 0 };
	VkImageSubresourceRange image_range = {};
	image_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	vkCmdClearDepthStencilImage(clear_buffer, render_target_depth_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_depth, 1, &image_range);

	devices_->EndSingleTimeCommands(clear_buffer);

	// transition buffers back to the correct format
	devices_->TransitionImageLayout(render_target_depth_image_, render_target_depth_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}