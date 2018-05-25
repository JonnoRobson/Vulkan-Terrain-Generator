#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "device.h"
#include "swap_chain.h"
#include "shader.h"
#include "texture_cache.h"
#include "camera.h"
#include "compute_shader.h"
#include "pipelines\buffer_visualisation_pipeline.h"
#include "pipelines\terrain_rendering_pipeline.h"
#include "pipelines\water_rendering_pipeline.h"
#include "terrain_generator.h"
#include "skybox.h"
#include "HDR.h"

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct ColorDataBuffer
{
	glm::vec4 rockColor;
	glm::vec4 snowColor;
	glm::vec4 fogColor;
	glm::vec4 waterColor;
};

class VulkanRenderer
{
public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain);
	void InitPipelines();
	void RenderScene();
	void Cleanup();
	
	void RecreateSwapChainFeatures();

	void SetCamera(Camera* camera) { render_camera_ = camera; }
	
	VulkanSwapChain* GetSwapChain() { return swap_chain_; }
	VkCommandPool GetCommandPool() { return command_pool_; }
	
	Texture* GetDefaultTexture() { return default_texture_; }

	inline void SetTextureDirectory(std::string dir) { texture_directory_ = dir; }
	inline std::string GetTextureDirectory() { return texture_directory_; }

	inline VkSemaphore GetRenderSemaphore() { return render_semaphore_; }
	inline VulkanTextureCache*	GetTextureCache() { return texture_cache_; }

	
protected:

	// command buffer creation functions
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateBufferVisualisationCommandBuffers();
	void CreateTerrainRenderingCommandBuffers();
	void CreateWaterRenderingCommandBuffers();

	// resource creation functions
	void CreateBuffers();
	void CreateSemaphores();
	void CreateShaders();

	// rendering functions
	void RenderVisualisation();
	void RenderTerrain();
	void RenderWater();
	std::vector<VkCommandBuffer> CheckTerrainVisibility();

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanTextureCache* texture_cache_;

	// buffer visualisation components
	VulkanShader* buffer_visualisation_shader_;
	BufferVisualisationPipeline* buffer_visualisation_pipeline_;
	VkCommandBuffer buffer_visualisation_command_buffer_;

	// terrain rendering components
	Mesh* terrain_mesh_;
	VulkanShader* terrain_shader_;
	TerrainRenderingPipeline* terrain_rendering_pipeline_;
	std::vector<VkCommandBuffer> terrain_rendering_command_buffers_;
	VkBuffer terrain_matrix_buffer_, terrain_lod_factors_buffer_, terrain_render_data_buffer_, fog_factors_buffer_, color_data_buffer_;
	VkDeviceMemory terrain_matrix_buffer_memory_, terrain_lod_factors_buffer_memory_, terrain_render_data_buffer_memory_, fog_factors_buffer_memory_, color_data_buffer_memory_;
	
	// water rendering components
	Mesh* water_mesh_;
	VulkanShader* water_shader_;
	WaterRenderingPipeline* water_rendering_pipeline_;
	VkCommandBuffer water_rendering_command_buffer_;
	VkBuffer water_matrix_buffer_, water_render_data_buffer_;
	VkDeviceMemory water_matrix_buffer_memory_, water_render_data_buffer_memory_;

	Skybox* skybox_;
	HDR* hdr_;
	TerrainGenerator* terrain_generator_;
	int current_chunk_x_, current_chunk_y_;
	TerrainMatrixData terrain_matrix_data_;

	Camera* render_camera_;
	Texture* default_texture_;
	Texture* mountain_texture_;
	Texture* sand_texture_;
	Texture* grass_texture_;
	VkSampler buffer_unnormalized_sampler_, buffer_normalized_sampler_, shadow_map_sampler_;

	VkQueue graphics_queue_;
	VkQueue compute_queue_;
	VkCommandPool command_pool_;
	VkSemaphore render_semaphore_, visualisation_semaphore_, terrain_rendering_semaphore_, water_rendering_semaphore_;

	std::string texture_directory_;
};

#endif