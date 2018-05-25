#include "compute_pipeline.h"

void VulkanComputePipeline::Init(VulkanDevices* devices)
{
	devices_ = devices;

	CreateDescriptorSet();
	CreatePipeline();
}

void VulkanComputePipeline::CleanUp()
{
	vkDestroyDescriptorSetLayout(devices_->GetLogicalDevice(), descriptor_set_layout_, nullptr);
	vkDestroyDescriptorPool(devices_->GetLogicalDevice(), descriptor_pool_, nullptr);
	vkDestroyPipelineLayout(devices_->GetLogicalDevice(), pipeline_layout_, nullptr);
	vkDestroyPipeline(devices_->GetLogicalDevice(), pipeline_, nullptr);
}

void VulkanComputePipeline::CreateDescriptorSet()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for (Descriptor& descriptor : descriptor_infos_)
		bindings.push_back(descriptor.layout_binding);

	// setup descriptor set layout creation info
	VkDescriptorSetLayoutCreateInfo layout_create_info = {};
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.bindingCount = static_cast<uint32_t>(descriptor_infos_.size());
	layout_create_info.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(devices_->GetLogicalDevice(), &layout_create_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline descriptor set layout!");
	}

	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (VkDescriptorSetLayoutBinding& binding : bindings)
	{
		bool found = false;

		// search for the binding in the existing list of bindings
		for (VkDescriptorPoolSize& pool_size : pool_sizes)
		{
			if (binding.descriptorType == pool_size.type)
			{
				// if binding is found increment binding count
				pool_size.descriptorCount += binding.descriptorCount;
				found = true;
				break;
			}
		}

		if (!found)
		{
			// if binding is not found create a new entry for it
			VkDescriptorPoolSize pool_size;
			pool_size.type = binding.descriptorType;
			pool_size.descriptorCount = binding.descriptorCount;
			pool_sizes.push_back(pool_size);
		}
	}

	// setup descriptor pool creation info
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 256;

	if (vkCreateDescriptorPool(devices_->GetLogicalDevice(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline descriptor pool!");
	}

	// setup descriptor set creation info
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool_;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_set_layout_;

	if (vkAllocateDescriptorSets(devices_->GetLogicalDevice(), &alloc_info, &descriptor_set_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate pipeline descriptor set!");
	}

	// setup write data for descriptor sets
	std::vector<VkWriteDescriptorSet> descriptor_writes = {};
	for (Descriptor& descriptor : descriptor_infos_)
	{
		VkWriteDescriptorSet descriptor_write = {};
		descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_write.dstSet = descriptor_set_;
		descriptor_write.dstBinding = descriptor.layout_binding.binding;
		descriptor_write.dstArrayElement = 0;
		descriptor_write.descriptorType = descriptor.layout_binding.descriptorType;
		descriptor_write.descriptorCount = descriptor.layout_binding.descriptorCount;

		if (descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			descriptor_write.pBufferInfo = descriptor.buffer_infos.data();
		}
		else if (descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
			descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
			descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
			descriptor.layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			descriptor_write.pImageInfo = descriptor.image_infos.data();
		}

		descriptor_writes.push_back(descriptor_write);
	}

	vkUpdateDescriptorSets(devices_->GetLogicalDevice(), static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
}

void VulkanComputePipeline::CreatePipeline()
{
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = 0;

	// create the pipeline layout
	if (vkCreatePipelineLayout(devices_->GetLogicalDevice(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup pipeline creation info
	VkComputePipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_info.stage = shader_->GetShaderStageInfo();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateComputePipelines(devices_->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create deferred compute pipeline!");
	}
}

void VulkanComputePipeline::AddTexture(uint32_t binding_location, Texture* texture)
{
	Descriptor texture_descriptor = {};

	// setup image info
	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = texture->GetImageView();
	image_info.sampler = texture->GetSampler();
	texture_descriptor.image_infos.push_back(image_info);

	// setup descriptor layout info
	texture_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texture_descriptor.layout_binding.descriptorCount = 1;
	texture_descriptor.layout_binding.binding = binding_location;
	texture_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanComputePipeline::AddTexture(uint32_t binding_location, VkImageView image)
{
	Descriptor texture_descriptor = {};

	// setup image info
	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = image;
	image_info.sampler = nullptr;
	texture_descriptor.image_infos.push_back(image_info);

	// setup descriptor layout info
	texture_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	texture_descriptor.layout_binding.descriptorCount = 1;
	texture_descriptor.layout_binding.binding = binding_location;
	texture_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanComputePipeline::AddTextureArray(uint32_t binding_location, std::vector<Texture*>& textures)
{
	Descriptor texture_descriptor = {};

	// setup image info
	for (Texture* texture : textures)
	{
		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture->GetImageView();
		image_info.sampler = VK_NULL_HANDLE;
		texture_descriptor.image_infos.push_back(image_info);
	}

	// setup descriptor layout info
	texture_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	texture_descriptor.layout_binding.descriptorCount = texture_descriptor.image_infos.size();
	texture_descriptor.layout_binding.binding = binding_location;
	texture_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanComputePipeline::AddTextureArray(uint32_t binding_location, std::vector<VkImageView>& textures)
{
	Descriptor texture_descriptor = {};

	// setup image info
	for (VkImageView texture : textures)
	{
		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture;
		image_info.sampler = VK_NULL_HANDLE;
		texture_descriptor.image_infos.push_back(image_info);
	}

	// setup descriptor layout info
	texture_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	texture_descriptor.layout_binding.descriptorCount = texture_descriptor.image_infos.size();
	texture_descriptor.layout_binding.binding = binding_location;
	texture_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}


void VulkanComputePipeline::AddSampler(uint32_t binding_location, VkSampler sampler)
{
	Descriptor texture_descriptor = {};

	// setup image info
	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = VK_NULL_HANDLE;
	image_info.sampler = sampler;
	texture_descriptor.image_infos.push_back(image_info);

	// setup descriptor layout info
	texture_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	texture_descriptor.layout_binding.descriptorCount = 1;
	texture_descriptor.layout_binding.binding = binding_location;
	texture_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanComputePipeline::AddUniformBuffer(uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size)
{
	Descriptor buffer_descriptor = {};

	// setup buffer info
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = buffer;
	buffer_info.offset = 0;
	buffer_info.range = buffer_size;
	buffer_descriptor.buffer_infos.push_back(buffer_info);

	// setup descriptor layout info
	buffer_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	buffer_descriptor.layout_binding.descriptorCount = 1;
	buffer_descriptor.layout_binding.binding = binding_location;
	buffer_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	buffer_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(buffer_descriptor);
}

void VulkanComputePipeline::AddStorageBuffer(uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size)
{
	Descriptor buffer_descriptor = {};

	// setup buffer info
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = buffer;
	buffer_info.offset = 0;
	buffer_info.range = buffer_size;
	buffer_descriptor.buffer_infos.push_back(buffer_info);

	// setup descriptor layout info
	buffer_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	buffer_descriptor.layout_binding.descriptorCount = 1;
	buffer_descriptor.layout_binding.binding = binding_location;
	buffer_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	buffer_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(buffer_descriptor);
}

void VulkanComputePipeline::AddStorageImage(uint32_t binding_location, VkImageView image)
{
	Descriptor image_descriptor = {};

	// setup image info
	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	image_info.imageView = image;
	image_info.sampler = VK_NULL_HANDLE;

	image_descriptor.image_infos.push_back(image_info);
	image_descriptor.layout_binding.binding = binding_location;
	image_descriptor.layout_binding.descriptorCount = 1;
	image_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	image_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	image_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(image_descriptor);
}

void VulkanComputePipeline::AddStorageImageArray(uint32_t binding_location, std::vector<VkImageView>& images)
{
	Descriptor image_descriptor = {};

	// setup image info
	for (VkImageView image : images)
	{
		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		image_info.imageView = image;
		image_info.sampler = VK_NULL_HANDLE;
		image_descriptor.image_infos.push_back(image_info);
	}

	// setup descriptor layout info
	image_descriptor.layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	image_descriptor.layout_binding.descriptorCount = image_descriptor.image_infos.size();
	image_descriptor.layout_binding.binding = binding_location;
	image_descriptor.layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	image_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(image_descriptor);
}