#ifndef _HDR_H_
#define _HDR_H_

#include <glm\glm.hpp>

#include "pipelines/ldr_suppress_pipeline.h"
#include "pipelines/gaussian_blur_pipeline.h"
#include "pipelines/tonemap_pipeline.h"
#include "render_target.h"

class HDR
{
public:
	struct GaussianBlurFactors
	{
		float radius;
		glm::vec2 direction;
		float padding;
	};

	struct TonemapFactors
	{
		float vignette_strength;
		float exposure_level;
		float gamma_level;
		float special_hdr;
	};

public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool);
	void Cleanup();
	void Render(VulkanSwapChain* swap_chain, VkSemaphore* wait_semaphore);

	inline VkSemaphore GetHDRSemaphore() { return hdr_semaphore_; }
	inline VulkanRenderTarget* DebugBuffer() { return ldr_suppress_scene_; }

	void CycleHDRMode();
	inline int GetHDRMode() { return hdr_mode_; }

protected:
	void InitPipelines(VulkanSwapChain* swap_chain);
	void InitShaders(VulkanSwapChain* swap_chain);
	void InitResources();
	void InitCommandBuffers(VkCommandPool command_pool);

protected:
	VulkanDevices* devices_;
	
	VkSampler buffer_sampler_;
	VulkanShader *ldr_suppress_shader_, *gaussian_blur_shader_, *tonemap_shader_;
	LDRSuppressPipeline* ldr_suppress_pipeline_;
	GaussianBlurPipeline* gaussian_blur_pipeline_[2];
	TonemapPipeline* tonemap_pipeline_;
	VulkanRenderTarget *ldr_suppress_scene_, *blur_scene_, *tonemap_scene_;
	VkBuffer gaussian_blur_factors_buffer_, tonemap_factors_buffer_;
	VkDeviceMemory gaussian_blur_factors_buffer_memory_, tonemap_factors_buffer_memory_;
	VkCommandBuffer ldr_suppress_command_buffer_, gaussian_blur_command_buffers_[2], tonemap_command_buffer_;
	VkSemaphore ldr_suppress_semaphore_, gaussian_blur_semaphore_[2], hdr_semaphore_;

	int hdr_mode_;
	TonemapFactors tonemap_factors_;
};

#endif