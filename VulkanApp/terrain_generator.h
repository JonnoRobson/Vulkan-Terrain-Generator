#ifndef _TERRAIN_GENERATOR_H_
#define _TERRAIN_GENERATOR_H_

#include <glm\glm.hpp>

#include "pipelines\heightmap_generation_pipeline.h"
#include "pipelines\watermap_generation_pipeline.h"
#include "render_target.h"

const int TERRAIN_SIZE = 1024;
const int TERRAIN_CHUNK_SIZE = 5;
const int TERRAIN_CHUNK_COUNT = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE;
const VkFormat IMAGE_FORMAT = VK_FORMAT_R16_SFLOAT;

const int WATER_SIZE = (TERRAIN_SIZE / 4) * TERRAIN_CHUNK_SIZE;

struct TerrainMatrixData
{
	glm::mat4 world[TERRAIN_CHUNK_COUNT];
	glm::mat4 view;
	glm::mat4 proj;
};

class TerrainGenerator
{
public:
	TerrainGenerator();

	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool, VkQueue compute_queue);
	void Cleanup();
	void GenerateHeightmap(int index);
	void GenerateWatermap();

	inline void SetPositionOffset(glm::vec2 offset) { fbm_generation_data_.position_offset = offset; }
	inline void SetSeed(float seed) { fbm_generation_data_.seed = seed; }

	inline VkImageView GetHeightmap(int index) { return heightmap_image_views_[index]; }
	inline std::vector<VkImageView> GetHeightmaps() { return heightmap_image_views_; }

	inline VkImageView GetWatermap() { return watermap_image_view_; }
	inline VkImageView GetWatermapSegment() { return watermap_segment_image_view_; }

	void MoveLeft(int current_chunk_x, int current_chunk_y);
	void MoveRight(int current_chunk_x, int current_chunk_y);
	void MoveUp(int current_chunk_x, int current_chunk_y);
	void MoveDown(int current_chunk_x, int current_chunk_y);

protected:
	void InitPipeline();
	void InitResources();
	void InitCommandBuffer(VkCommandPool command_pool);

	// water map shifts
	void WatermapLeftShift();
	void WatermapRightShift();
	void WatermapUpShift();
	void WatermapDownShift();

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VkQueue compute_queue_;

	// heightmap generation
	VkDeviceSize generation_data_size_;
	CombinedPerlinGenerationData combined_perlin_generation_data_;
	FbmGenerationData fbm_generation_data_;

	VulkanComputeShader* heightmap_generation_shader_;
	std::vector<HeightmapGenerationPipeline*> heightmap_generation_pipelines_;

	VkBuffer heightmap_data_buffer_;
	VkDeviceMemory heightmap_data_buffer_memory_;

	std::vector<VkCommandBuffer> heightmap_generation_command_buffers_;
	VkSemaphore heightmap_generation_semaphore_; 

	std::vector<VkImage> heightmap_images_;
	std::vector<VkDeviceMemory> heightmap_image_memorys_;
	std::vector<VkImageView> heightmap_image_views_;

	// water map generation
	VulkanComputeShader* watermap_generation_shader_;
	WatermapGenerationPipeline* watermap_generation_pipeline_;
	
	VkBuffer watermap_data_buffer_;
	VkDeviceMemory watermap_data_buffer_memory_;

	VkCommandBuffer watermap_generation_command_buffer_;
	VkSemaphore watermap_generation_sempahore_;

	VkImage watermap_image_;
	VkDeviceMemory watermap_image_memory_;
	VkImageView watermap_image_view_;

	VkImage watermap_segment_image_;
	VkDeviceMemory watermap_segment_image_memory_;
	VkImageView watermap_segment_image_view_;
};

#endif