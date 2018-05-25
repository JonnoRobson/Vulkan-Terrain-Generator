#include "pipeline.h"

#include <array>

VulkanPipeline::VulkanPipeline()
{
}

void VulkanPipeline::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	CreateDescriptorSet();
	CreateRenderPass();
	CreateFramebuffers();
	CreatePipeline();
}

void VulkanPipeline::CleanUp()
{
	vkDestroyDescriptorSetLayout(devices_->GetLogicalDevice(), descriptor_set_layout_, nullptr);
	vkDestroyDescriptorPool(devices_->GetLogicalDevice(), descriptor_pool_, nullptr);
	vkDestroyRenderPass(devices_->GetLogicalDevice(), render_pass_, nullptr);
	
	for (int i = 0; i < framebuffers_.size(); i++)
	{
		vkDestroyFramebuffer(devices_->GetLogicalDevice(), framebuffers_[i], nullptr);
	}

	vkDestroyPipelineLayout(devices_->GetLogicalDevice(), pipeline_layout_, nullptr);
	vkDestroyPipeline(devices_->GetLogicalDevice(), pipeline_, nullptr);
}

void VulkanPipeline::CreateDescriptorSet()
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

void VulkanPipeline::CreateRenderPass()
{
	// setup the color buffer attachment
	VkAttachmentDescription color_attachment = {};
	color_attachment.format = swap_chain_->GetSwapChainImageFormat();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// setup the subpass attachment description
	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the depth buffer attachment
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = swap_chain_->FindDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass description
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	// setup the render pass dependancy description
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// setup the render pass description
	std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	render_pass_info.pAttachments = attachments.data();
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	if (vkCreateRenderPass(devices_->GetLogicalDevice(), &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void VulkanPipeline ::CreateFramebuffers()
{
	std::vector<VkImageView>& image_views = swap_chain_->GetSwapChainImageViews();
	framebuffers_.resize(image_views.size());

	for (size_t i = 0; i < image_views.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { image_views[i], swap_chain_->GetDepthImageView() };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass_;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = swap_chain_->GetSwapChainExtent().width;
		framebuffer_info.height = swap_chain_->GetSwapChainExtent().height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(devices_->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void VulkanPipeline::CreatePipeline()
{
	// setup pipeline layout creation info
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
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = shader_->GetShaderStageCount();
	pipeline_info.pStages = shader_->GetShaderStageInfo().data();
	pipeline_info.pVertexInputState = &shader_->GetVertexInputDescription();
	pipeline_info.pInputAssemblyState = &shader_->GetInputAssemblyDescription();
	pipeline_info.pViewportState = &shader_->GetViewportStateDescription();
	pipeline_info.pRasterizationState = &shader_->GetRasterizerStateDescription();
	pipeline_info.pMultisampleState = &shader_->GetMultisampleStateDescription();
	pipeline_info.pDepthStencilState = &shader_->GetDepthStencilStateDescription();
	pipeline_info.pColorBlendState = &shader_->GetBlendStateDescription();
	pipeline_info.pDynamicState = &shader_->GetDynamicStateDescription();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateGraphicsPipelines(devices_->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}
}

void VulkanPipeline::RecreateSwapChainFeatures()
{
	// free the previous swap chain features
	vkDestroyRenderPass(devices_->GetLogicalDevice(), render_pass_, nullptr);

	for (size_t i = 0; i < framebuffers_.size(); i++)
	{
		vkDestroyFramebuffer(devices_->GetLogicalDevice(), framebuffers_[i], nullptr);
	}

	// create the new swap chain features
	CreateRenderPass();
	CreateFramebuffers();
}

void VulkanPipeline::AddTexture(VkShaderStageFlags stage_flags, uint32_t binding_location, Texture* texture)
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
	texture_descriptor.layout_binding.stageFlags = stage_flags;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanPipeline::AddTexture(VkShaderStageFlags stage_flags, uint32_t binding_location, VkImageView image)
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
	texture_descriptor.layout_binding.stageFlags = stage_flags;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanPipeline::AddTextureArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<Texture*>& textures)
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
	texture_descriptor.layout_binding.stageFlags = stage_flags;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanPipeline::AddTextureArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<VkImageView>& textures)
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
	texture_descriptor.layout_binding.stageFlags = stage_flags;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}


void VulkanPipeline::AddSampler(VkShaderStageFlags stage_flags, uint32_t binding_location, VkSampler sampler)
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
	texture_descriptor.layout_binding.stageFlags = stage_flags;
	texture_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(texture_descriptor);
}

void VulkanPipeline::AddUniformBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size)
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
	buffer_descriptor.layout_binding.stageFlags = stage_flags;
	buffer_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(buffer_descriptor);
}

void VulkanPipeline::AddStorageBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size)
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
	buffer_descriptor.layout_binding.stageFlags = stage_flags;
	buffer_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(buffer_descriptor);
}

void VulkanPipeline::AddStorageImage(VkShaderStageFlags stage_flags, uint32_t binding_location, VkImageView image)
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
	image_descriptor.layout_binding.stageFlags = stage_flags;
	image_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(image_descriptor);
}

void VulkanPipeline::AddStorageImageArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<VkImageView>& images)
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
	image_descriptor.layout_binding.stageFlags = stage_flags;
	image_descriptor.layout_binding.pImmutableSamplers = nullptr;

	descriptor_infos_.push_back(image_descriptor);
}

void VulkanPipeline::RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index)
{
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_;
	render_pass_info.framebuffer = framebuffers_[buffer_index];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = swap_chain_->GetSwapChainExtent();
	render_pass_info.clearValueCount = 0;
	render_pass_info.pClearValues = nullptr;
	
	// create pipleine commands
	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
	
	// set the dynamic viewport data
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(swap_chain_->GetSwapChainExtent().width);
	viewport.height = (float)(swap_chain_->GetSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// set the dynamic scissor data
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetSwapChainExtent();
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	// bind the descriptor set to the pipeline
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);
}