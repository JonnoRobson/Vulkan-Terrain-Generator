#ifndef _SKYBOX_PIPELINE_H_
#define _SKYBOX_PIPELINE_H_

#include "pipelines\pipeline.h"
#include "mesh.h"
#include "camera.h"

class SkyboxPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();
};

class Skybox
{
public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool, VkBuffer color_data_buffer);
	void Cleanup();
	void Render(Camera* camera);

	inline VkSemaphore GetRenderSemaphore() { return render_semaphore_; }

protected:
	void InitPipeline(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkBuffer color_data_buffer);
	void InitCommandBuffer(VkCommandPool command_pool);
	void InitResources();

protected:
	VulkanDevices* devices_;

	VulkanShader* skybox_shader_;
	SkyboxPipeline* skybox_pipeline_;
	Mesh* skybox_mesh_;
	Texture* skybox_texture_;
	VkCommandBuffer skybox_command_buffer_;
	VkSemaphore render_semaphore_;
	VkBuffer matrix_buffer_;
	VkDeviceMemory matrix_buffer_memory_;

};

#endif