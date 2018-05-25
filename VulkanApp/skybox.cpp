#include "skybox.h"
#include "renderer.h"
#include <array>

void SkyboxPipeline::RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index)
{
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_;
	render_pass_info.framebuffer = framebuffers_[0];
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

void SkyboxPipeline::CreatePipeline()
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

	// setup rasterizer creation info
	VkPipelineRasterizationStateCreateInfo rasterizer_state = {};
	rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state.depthClampEnable = VK_FALSE;
	rasterizer_state.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state.lineWidth = 1.0f;
	rasterizer_state.cullMode = VK_CULL_MODE_FRONT_BIT;
	rasterizer_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_state.depthBiasEnable = VK_FALSE;
	rasterizer_state.depthBiasConstantFactor = 0.0f;
	rasterizer_state.depthBiasClamp = 0.0f;
	rasterizer_state.depthBiasSlopeFactor = 0.0f;

	// setup pipeline creation info
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = shader_->GetShaderStageCount();
	pipeline_info.pStages = shader_->GetShaderStageInfo().data();
	pipeline_info.pVertexInputState = &shader_->GetVertexInputDescription();
	pipeline_info.pInputAssemblyState = &shader_->GetInputAssemblyDescription();
	pipeline_info.pViewportState = &shader_->GetViewportStateDescription();
	pipeline_info.pRasterizationState = &rasterizer_state;
	pipeline_info.pMultisampleState = &shader_->GetMultisampleStateDescription();
	pipeline_info.pDepthStencilState = nullptr;
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

void SkyboxPipeline::CreateRenderPass()
{
	// setup the color buffer attachment
	VkAttachmentDescription color_attachment = {};
	color_attachment.format = swap_chain_->GetIntermediateImageFormat();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the subpass description
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = nullptr;

	// setup the render pass dependancy description
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// setup the render pass description
	std::array<VkAttachmentDescription, 1> attachments = { color_attachment };

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

void SkyboxPipeline::CreateFramebuffers()
{
	VkImageView image_view = swap_chain_->GetIntermediateImageView();
	framebuffers_.resize(1);

	std::array<VkImageView, 1> attachments = { image_view };

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = render_pass_;
	framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebuffer_info.pAttachments = attachments.data();
	framebuffer_info.width = swap_chain_->GetSwapChainExtent().width;
	framebuffer_info.height = swap_chain_->GetSwapChainExtent().height;
	framebuffer_info.layers = 1;

	if (vkCreateFramebuffer(devices_->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffers_[0]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create framebuffer!");
	}
}

void Skybox::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool, VkBuffer color_data_buffer)
{
	devices_ = devices;

	InitResources();
	InitPipeline(devices, swap_chain, color_data_buffer);
	InitCommandBuffer(command_pool);
}

void Skybox::Cleanup()
{
	// clean up buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), matrix_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_, nullptr);
	
	// clean up mesh
	delete skybox_mesh_;
	skybox_mesh_ = nullptr;

	// clean up textures
	skybox_texture_->Cleanup();
	delete skybox_texture_;
	skybox_texture_ = nullptr;

	// clean up pipeline
	skybox_pipeline_->CleanUp();
	delete skybox_pipeline_;
	skybox_pipeline_ = nullptr;

	// clean up shader
	skybox_shader_->Cleanup();
	delete skybox_shader_;
	skybox_shader_ = nullptr;

	// clean up semaphore
	vkDestroySemaphore(devices_->GetLogicalDevice(), render_semaphore_, nullptr);
}

void Skybox::Render(Camera* camera)
{
	// copy the matrix data to the buffer
	UniformBufferObject ubo = {};
	ubo.model = glm::translate(camera->GetPosition());
	ubo.view = camera->GetViewMatrix();
	ubo.proj = camera->GetProjectionMatrix();

	devices_->CopyDataToBuffer(matrix_buffer_memory_, &ubo, sizeof(UniformBufferObject));
	

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &skybox_command_buffer_;

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkQueue graphics_queue;
	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue);

	VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}
}

void Skybox::InitPipeline(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkBuffer color_data_buffer)
{
	// create the render semaphore
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &render_semaphore_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}

	// create the matrix buffer
	devices_->CreateBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, matrix_buffer_, matrix_buffer_memory_);
	
	// create the skybox shader
	skybox_shader_ = new VulkanShader();
	skybox_shader_->Init(devices, swap_chain, "../res/shaders/skybox.vert.spv", "", "", "", "../res/shaders/skybox.frag.spv");

	// initialize the skybox pipeline
	skybox_pipeline_ = new SkyboxPipeline();
	skybox_pipeline_->SetShader(skybox_shader_);
	skybox_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	skybox_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, skybox_texture_->GetSampler());
	skybox_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 2, skybox_texture_->GetImageView());
	skybox_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, color_data_buffer, sizeof(ColorDataBuffer));
	skybox_pipeline_->Init(devices, swap_chain);
}

void Skybox::InitCommandBuffer(VkCommandPool command_pool)
{
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &skybox_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}


	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(skybox_command_buffer_, &begin_info);

	skybox_pipeline_->RecordCommands(skybox_command_buffer_, 0);

	skybox_mesh_->RecordRenderCommands(skybox_command_buffer_);

	vkCmdEndRenderPass(skybox_command_buffer_);

	if (vkEndCommandBuffer(skybox_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record skybox command buffer!");
	}
}

void Skybox::InitResources()
{
	// init the skybox texture
	skybox_texture_ = new Texture();
	skybox_texture_->Init(devices_, "../res/textures/skybox.png");

	// init the skybox mesh
	skybox_mesh_ = new Mesh();
	skybox_mesh_->CreateModelMesh(devices_, "../res/models/skybox.obj");
}