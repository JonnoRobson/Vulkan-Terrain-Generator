#include "HDR.h"

void HDR::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool)
{
	devices_ = devices;

	InitResources();
	InitShaders(swap_chain);
	InitPipelines(swap_chain);
	InitCommandBuffers(command_pool);
}

void HDR::Cleanup()
{
	// clean up shaders
	ldr_suppress_shader_->Cleanup();
	delete ldr_suppress_shader_;
	ldr_suppress_shader_ = nullptr;

	gaussian_blur_shader_->Cleanup();
	delete gaussian_blur_shader_;
	gaussian_blur_shader_ = nullptr;

	tonemap_shader_->Cleanup();
	delete tonemap_shader_;
	tonemap_shader_ = nullptr;

	// clean up samplers
	vkDestroySampler(devices_->GetLogicalDevice(), buffer_sampler_, nullptr);

	// clean up buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), gaussian_blur_factors_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), gaussian_blur_factors_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), tonemap_factors_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), tonemap_factors_buffer_memory_, nullptr);

	// clean up render targets
	ldr_suppress_scene_->Cleanup();
	delete ldr_suppress_scene_;
	ldr_suppress_scene_ = nullptr;

	blur_scene_->Cleanup();
	delete blur_scene_;
	blur_scene_ = nullptr;

	tonemap_scene_->Cleanup();
	delete tonemap_scene_;
	tonemap_scene_ = nullptr;

	// clean up pipelines
	ldr_suppress_pipeline_->CleanUp();
	delete ldr_suppress_pipeline_;
	ldr_suppress_pipeline_ = nullptr;

	gaussian_blur_pipeline_[0]->CleanUp();
	delete gaussian_blur_pipeline_[0];
	gaussian_blur_pipeline_[0] = nullptr;

	gaussian_blur_pipeline_[1]->CleanUp();
	delete gaussian_blur_pipeline_[1];
	gaussian_blur_pipeline_[1] = nullptr;

	tonemap_pipeline_->CleanUp();
	delete tonemap_pipeline_;
	tonemap_pipeline_ = nullptr;

	// clean up semaphores
	vkDestroySemaphore(devices_->GetLogicalDevice(), ldr_suppress_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), gaussian_blur_semaphore_[0], nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), gaussian_blur_semaphore_[1], nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), hdr_semaphore_, nullptr);
}

void HDR::Render(VulkanSwapChain* swap_chain, VkSemaphore* wait_semaphore)
{
	VkQueue graphics_queue;
	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue);

	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit_info = {};
	VkSemaphore signal_semaphores[1];

	// clear the render targets
	ldr_suppress_scene_->ClearImage();
	blur_scene_->ClearImage();
	tonemap_scene_->ClearImage();

	// suppress low dynamic range pixels and downscaled image
	// submit the draw command buffer
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphore;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &ldr_suppress_command_buffer_;

	signal_semaphores[0] = ldr_suppress_semaphore_;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}

	// apply a gaussian blur filter to the image
	// horizontal filter
	GaussianBlurFactors blur_factors =
	{
		8.0f,
		glm::vec2(1.0f, 0.0f),
		0.0f
	};
	devices_->CopyDataToBuffer(gaussian_blur_factors_buffer_memory_, &blur_factors, sizeof(GaussianBlurFactors));

	// submit the draw command buffer
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &ldr_suppress_semaphore_;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &gaussian_blur_command_buffers_[0];

	signal_semaphores[0] = gaussian_blur_semaphore_[0];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}

	// vertical filter
	blur_factors.direction = glm::vec2(0.0f, 1.0f);
	devices_->CopyDataToBuffer(gaussian_blur_factors_buffer_memory_, &blur_factors, sizeof(GaussianBlurFactors));

	// submit the draw command buffer
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &gaussian_blur_semaphore_[0];
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &gaussian_blur_command_buffers_[1];

	signal_semaphores[0] = gaussian_blur_semaphore_[1];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}

	// tonemap the blurred image with the original
	// update tonemapping data
	devices_->CopyDataToBuffer(tonemap_factors_buffer_memory_, &tonemap_factors_, sizeof(TonemapFactors));

	// submit the draw command buffer
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &gaussian_blur_semaphore_[1];
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &tonemap_command_buffer_;

	signal_semaphores[0] = hdr_semaphore_;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}

	// wait until tonemapping is complete
	vkQueueWaitIdle(graphics_queue);

	// copy the tonemapped image to the intermediate image
	swap_chain->CopyToIntermediateImage(tonemap_scene_->GetImages()[0], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void HDR::InitPipelines(VulkanSwapChain* swap_chain)
{
	VkExtent2D swap_chain_dimensions = swap_chain->GetSwapChainExtent();

	// initialize the HDR render targets
	ldr_suppress_scene_ = new VulkanRenderTarget();
	ldr_suppress_scene_->Init(devices_, swap_chain->GetIntermediateImageFormat(), swap_chain_dimensions.width / 2, swap_chain_dimensions.height / 2, 1, false);

	blur_scene_ = new VulkanRenderTarget();
	blur_scene_->Init(devices_, swap_chain->GetIntermediateImageFormat(), swap_chain_dimensions.width / 2, swap_chain_dimensions.height / 2, 2, false);

	tonemap_scene_ = new VulkanRenderTarget();
	tonemap_scene_->Init(devices_, swap_chain->GetIntermediateImageFormat(), swap_chain_dimensions.width, swap_chain_dimensions.height, 1, false);

	// initialize the ldr suppresssion pipeline
	ldr_suppress_pipeline_ = new LDRSuppressPipeline();
	ldr_suppress_pipeline_->SetShader(ldr_suppress_shader_);
	ldr_suppress_pipeline_->SetOutputImage(ldr_suppress_scene_->GetImageViews()[0], ldr_suppress_scene_->GetRenderTargetFormat(), swap_chain_dimensions.width / 2, swap_chain_dimensions.height / 2);
	ldr_suppress_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0, buffer_sampler_);
	ldr_suppress_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 1, swap_chain->GetIntermediateImageView());
	ldr_suppress_pipeline_->Init(devices_, swap_chain);

	// initialize the gaussian blur pipelines
	gaussian_blur_pipeline_[0] = new GaussianBlurPipeline();
	gaussian_blur_pipeline_[0]->SetShader(gaussian_blur_shader_);
	gaussian_blur_pipeline_[0]->SetOutputImage(blur_scene_->GetImageViews()[0], blur_scene_->GetRenderTargetFormat(), swap_chain_dimensions.width / 2, swap_chain_dimensions.height / 2);
	gaussian_blur_pipeline_[0]->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, gaussian_blur_factors_buffer_, sizeof(GaussianBlurFactors));
	gaussian_blur_pipeline_[0]->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, buffer_sampler_);
	gaussian_blur_pipeline_[0]->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 2, ldr_suppress_scene_->GetImageViews()[0]);
	gaussian_blur_pipeline_[0]->Init(devices_, swap_chain);

	gaussian_blur_pipeline_[1] = new GaussianBlurPipeline();
	gaussian_blur_pipeline_[1]->SetShader(gaussian_blur_shader_);
	gaussian_blur_pipeline_[1]->SetOutputImage(blur_scene_->GetImageViews()[1], blur_scene_->GetRenderTargetFormat(), swap_chain_dimensions.width / 2, swap_chain_dimensions.height / 2);
	gaussian_blur_pipeline_[1]->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, gaussian_blur_factors_buffer_, sizeof(GaussianBlurFactors));
	gaussian_blur_pipeline_[1]->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, buffer_sampler_);
	gaussian_blur_pipeline_[1]->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 2, blur_scene_->GetImageViews()[0]);
	gaussian_blur_pipeline_[1]->Init(devices_, swap_chain);
	
	// intialize the tonemap pipeline
	tonemap_pipeline_ = new TonemapPipeline();
	tonemap_pipeline_->SetShader(tonemap_shader_);
	tonemap_pipeline_->SetOutputImage(tonemap_scene_->GetImageViews()[0], tonemap_scene_->GetRenderTargetFormat(), swap_chain_dimensions.width, swap_chain_dimensions.height);
	tonemap_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, tonemap_factors_buffer_, sizeof(TonemapFactors));
	tonemap_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, buffer_sampler_);
	tonemap_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 2, swap_chain->GetIntermediateImageView());
	tonemap_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 3, blur_scene_->GetImageViews()[1]);
	tonemap_pipeline_->Init(devices_, swap_chain);
}

void HDR::InitShaders(VulkanSwapChain* swap_chain)
{
	ldr_suppress_shader_ = new VulkanShader();
	ldr_suppress_shader_->Init(devices_, swap_chain, "../res/shaders/screen_space.vert.spv", "", "", "", "../res/shaders/ldr_suppress.frag.spv");

	gaussian_blur_shader_ = new VulkanShader();
	gaussian_blur_shader_->Init(devices_, swap_chain, "../res/shaders/screen_space.vert.spv", "", "", "", "../res/shaders/gaussian_blur.frag.spv");

	tonemap_shader_ = new VulkanShader();
	tonemap_shader_->Init(devices_, swap_chain, "../res/shaders/screen_space.vert.spv", "", "", "", "../res/shaders/tonemap.frag.spv");
}

void HDR::InitResources()
{
	// start as hdr on normal
	hdr_mode_ = 1;

	// initialize the sampler that will be used for buffers
	// initialize the g buffer sampler
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &buffer_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer sampler!");
	}

	// intialize the blur factors buffer
	devices_->CreateBuffer(sizeof(GaussianBlurFactors), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gaussian_blur_factors_buffer_, gaussian_blur_factors_buffer_memory_);

	// initialize the tonemap factors buffer
	devices_->CreateBuffer(sizeof(TonemapFactors), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tonemap_factors_buffer_, tonemap_factors_buffer_memory_);

	// send the tonemap factors to the buffer
	tonemap_factors_ =
	{
		3.0f,	// vignette strength
		2.5f,	// exposure level
		0.75f,	// gamma_level
		0.0f	// use special hdr
	};

	devices_->CopyDataToBuffer(tonemap_factors_buffer_memory_, &tonemap_factors_, sizeof(TonemapFactors));

	// initialize semaphores
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &ldr_suppress_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &gaussian_blur_semaphore_[0]) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &gaussian_blur_semaphore_[1]) != VK_SUCCESS || 
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &hdr_semaphore_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create semaphores!");
	}
}

void HDR::InitCommandBuffers(VkCommandPool command_pool)
{
	// generic record info
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	// generic allocate info
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	// init ldr suppress command buffer
	allocate_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &ldr_suppress_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate ldr suppression command buffers!");
	}


	vkBeginCommandBuffer(ldr_suppress_command_buffer_, &begin_info);

	ldr_suppress_pipeline_->RecordCommands(ldr_suppress_command_buffer_, 0);

	vkCmdEndRenderPass(ldr_suppress_command_buffer_);

	if (vkEndCommandBuffer(ldr_suppress_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record ldr suppression command buffer!");
	}

	// init gaussian blur command buffers
	allocate_info.commandBufferCount = 2;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, gaussian_blur_command_buffers_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate gaussian blur command buffers!");
	}

	for (int i = 0; i < 2; i++)
	{
		vkBeginCommandBuffer(gaussian_blur_command_buffers_[i], &begin_info);

		gaussian_blur_pipeline_[i]->RecordCommands(gaussian_blur_command_buffers_[i], 0);

		vkCmdEndRenderPass(gaussian_blur_command_buffers_[i]);

		if (vkEndCommandBuffer(gaussian_blur_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record gaussian blur command buffer!");
		}
	}

	// init tonemap command buffer
	allocate_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &tonemap_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate tonemap command buffers!");
	}


	vkBeginCommandBuffer(tonemap_command_buffer_, &begin_info);

	tonemap_pipeline_->RecordCommands(tonemap_command_buffer_, 0);

	vkCmdEndRenderPass(tonemap_command_buffer_);

	if (vkEndCommandBuffer(tonemap_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record tonemap command buffer!");
	}
}

void HDR::CycleHDRMode()
{
	if (hdr_mode_ == 0)
	{
		hdr_mode_ = 1;
		tonemap_factors_.special_hdr = 0;
	}
	else if (hdr_mode_ == 1)
	{
		hdr_mode_ = 2;
		tonemap_factors_.special_hdr = 1;
	}
	else if (hdr_mode_ == 2)
	{
		hdr_mode_ = 0;
	}
}