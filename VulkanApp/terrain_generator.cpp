#include "terrain_generator.h"
#include <chrono>
#include <math.h>
#include <iostream>

TerrainGenerator::TerrainGenerator()
{
	srand(time(NULL));
	float seed = rand();

	combined_perlin_generation_data_ =
	{
		{0.0f, 0.0f},	// offset
		2.0f,			// shape sample width
		1.0f,			// shape intensity
		16.0f,			// detail sample width
		1.0f,			// detail intensity
		0.5f,			// interpolation value
		0.0f
	};

	fbm_generation_data_ =
	{
		{0.0f, 0.0f},	// offset
		8.0f,			// octaves
		0.75f,			// amplitude
		3.0f,			// frequency
		seed,			// seed
		8.0f,			// filter_width
		TERRAIN_SIZE	// padding
	};
}

void TerrainGenerator::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool, VkQueue compute_queue)
{
	devices_ = devices;
	swap_chain_ = swap_chain;
	compute_queue_ = compute_queue;

	InitResources();
	InitPipeline();
	InitCommandBuffer(command_pool);
}

void TerrainGenerator::Cleanup()
{
	// clean up buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), heightmap_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), heightmap_data_buffer_memory_, nullptr);
	vkDestroyBuffer(devices_->GetLogicalDevice(), watermap_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), watermap_data_buffer_memory_, nullptr);

	// clean up heightmap image
	for (int i = 0; i < TERRAIN_CHUNK_COUNT; i++)
	{
		vkDestroyImage(devices_->GetLogicalDevice(), heightmap_images_[i], nullptr);
		vkDestroyImageView(devices_->GetLogicalDevice(), heightmap_image_views_[i], nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), heightmap_image_memorys_[i], nullptr);

		// clean up pipelines
		heightmap_generation_pipelines_[i]->CleanUp();
		delete heightmap_generation_pipelines_[i];
		heightmap_generation_pipelines_[i] = nullptr;
	}

	// clean up watermap image
	vkDestroyImage(devices_->GetLogicalDevice(), watermap_image_, nullptr);
	vkDestroyImageView(devices_->GetLogicalDevice(), watermap_image_view_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), watermap_image_memory_, nullptr);

	vkDestroyImage(devices_->GetLogicalDevice(), watermap_segment_image_, nullptr);
	vkDestroyImageView(devices_->GetLogicalDevice(), watermap_segment_image_view_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), watermap_segment_image_memory_, nullptr);

	// clean up watermap generation pipeline
	watermap_generation_pipeline_->CleanUp();
	delete watermap_generation_pipeline_;
	watermap_generation_pipeline_ = nullptr;

	// clean up shaders
	heightmap_generation_shader_->Cleanup();
	delete heightmap_generation_shader_;
	heightmap_generation_shader_ = nullptr;

	watermap_generation_shader_->Cleanup();
	delete watermap_generation_shader_;
	watermap_generation_shader_ = nullptr;

	// clean up semaphore
	vkDestroySemaphore(devices_->GetLogicalDevice(), heightmap_generation_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), watermap_generation_sempahore_, nullptr);
}

void TerrainGenerator::GenerateHeightmap(int index)
{
	// update the terrain generation buffer with the new data
	devices_->CopyDataToBuffer(heightmap_data_buffer_memory_, &fbm_generation_data_, generation_data_size_);

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &heightmap_generation_command_buffers_[index];

	VkSemaphore signal_semaphores[] = { heightmap_generation_semaphore_ };
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = nullptr;

	auto begin = std::chrono::high_resolution_clock::now();

	VkResult result = vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit heightmap generation command buffer!");
	}

	vkQueueWaitIdle(compute_queue_);

	auto end = std::chrono::high_resolution_clock::now();

	float calc_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

	//std::cout << calc_time;
	//std::cout << "\n";
}

void TerrainGenerator::GenerateWatermap()
{
	// update the terrain generation buffer with the new data
	WaterGenerationFactors water_factors = {};
	water_factors.position_offset = glm::vec2(0.0f, 0.0f);
	water_factors.seed = fbm_generation_data_.seed;
	water_factors.size = WATER_SIZE;
	water_factors.terrain_size = TERRAIN_SIZE;
	water_factors.terrain_chunk_size = TERRAIN_CHUNK_SIZE;
	water_factors.height_offset = 1.0f;
	water_factors.decay_value = 0.01f;
	water_factors.water_level = 0.05f;
	water_factors.padding = glm::vec3(0.0f);

	devices_->CopyDataToBuffer(watermap_data_buffer_memory_, &water_factors, sizeof(WaterGenerationFactors));

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &watermap_generation_command_buffer_;

	VkSemaphore signal_semaphores[] = { watermap_generation_sempahore_ };
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = nullptr;

	auto begin = std::chrono::high_resolution_clock::now();
	VkResult result = vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit watermap generation command buffer!");
	}

	vkQueueWaitIdle(compute_queue_);
}

void TerrainGenerator::MoveLeft(int current_chunk_x, int current_chunk_y)
{
	// move existing chunks one to the right
	VkOffset3D heightmap_size = { TERRAIN_SIZE, TERRAIN_SIZE, 1 };

	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		for (int x = TERRAIN_CHUNK_SIZE - 2; x >= 0; x--)
		{
			int src_index = x + (y * TERRAIN_CHUNK_SIZE);
			int dst_index = src_index + 1;

			devices_->CopyImage(heightmap_images_[src_index], heightmap_images_[dst_index], heightmap_size);
		}
	}

	// generate new chunks
	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		int index = (y * TERRAIN_CHUNK_SIZE);

		int chunk_x = current_chunk_x - (TERRAIN_CHUNK_SIZE / 2);
		int chunk_y = current_chunk_y + (TERRAIN_CHUNK_SIZE / 2) - y;

		float offset_x = chunk_x * TERRAIN_SIZE;
		float offset_y = chunk_y * TERRAIN_SIZE;
		
		SetPositionOffset(glm::vec2(offset_x, offset_y));
		GenerateHeightmap(index);
	}

	// generate the new watermap
	WatermapLeftShift();
	GenerateWatermap();
}

void TerrainGenerator::MoveRight(int current_chunk_x, int current_chunk_y)
{
	// move existing chunks one to the left
	VkOffset3D heightmap_size = { TERRAIN_SIZE, TERRAIN_SIZE, 1 };

	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		for (int x = 1; x < TERRAIN_CHUNK_SIZE; x++)
		{
			int src_index = (x) + (y * TERRAIN_CHUNK_SIZE);
			int dst_index = src_index - 1;

			devices_->CopyImage(heightmap_images_[src_index], heightmap_images_[dst_index], heightmap_size);
		}
	}

	// generate new chunks
	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		int index = (TERRAIN_CHUNK_SIZE - 1) + (y * TERRAIN_CHUNK_SIZE);

		int chunk_x = current_chunk_x + (TERRAIN_CHUNK_SIZE / 2);
		int chunk_y = current_chunk_y + (TERRAIN_CHUNK_SIZE / 2) - y;

		float offset_x = chunk_x * TERRAIN_SIZE;
		float offset_y = chunk_y * TERRAIN_SIZE;

		SetPositionOffset(glm::vec2(offset_x, offset_y));

		GenerateHeightmap(index);
	}

	// generate the new watermap
	WatermapRightShift();
	GenerateWatermap();
}

void TerrainGenerator::MoveUp(int current_chunk_x, int current_chunk_y)
{
	// move existing chunks one down
	VkOffset3D heightmap_size = { TERRAIN_SIZE, TERRAIN_SIZE, 1 };

	for (int y = TERRAIN_CHUNK_SIZE - 2; y >= 0; y--)
	{
		for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
		{
			int src_index = x + (y * TERRAIN_CHUNK_SIZE);
			int dst_index = src_index + TERRAIN_CHUNK_SIZE;

			devices_->CopyImage(heightmap_images_[src_index], heightmap_images_[dst_index], heightmap_size);
		}
	}

	// generate new chunks
	for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
	{
		int index = x;

		int chunk_x = current_chunk_x - (TERRAIN_CHUNK_SIZE / 2) + x;
		int chunk_y = current_chunk_y + (TERRAIN_CHUNK_SIZE / 2);

		float offset_x = chunk_x * TERRAIN_SIZE;
		float offset_y = chunk_y * TERRAIN_SIZE;

		SetPositionOffset(glm::vec2(offset_x, offset_y));

		GenerateHeightmap(index);
	}

	// generate the new watermap
	WatermapUpShift();
	GenerateWatermap();
}

void TerrainGenerator::MoveDown(int current_chunk_x, int current_chunk_y)
{
	// move existing chunks one up
	VkOffset3D heightmap_size = { TERRAIN_SIZE, TERRAIN_SIZE, 1 };

	for (int y = 1; y < TERRAIN_CHUNK_SIZE ; y++)
	{
		for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
		{
			int src_index = x + (y * TERRAIN_CHUNK_SIZE);
			int dst_index = src_index - TERRAIN_CHUNK_SIZE;

			devices_->CopyImage(heightmap_images_[src_index], heightmap_images_[dst_index], heightmap_size);
		}
	}

	// generate new chunks
	for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
	{
		int index = x + (TERRAIN_CHUNK_SIZE * (TERRAIN_CHUNK_SIZE - 1));

		int chunk_x = current_chunk_x - (TERRAIN_CHUNK_SIZE / 2) + x;
		int chunk_y = current_chunk_y - (TERRAIN_CHUNK_SIZE / 2);

		float offset_x = chunk_x * TERRAIN_SIZE;
		float offset_y = chunk_y * TERRAIN_SIZE;

		SetPositionOffset(glm::vec2(offset_x, offset_y));

		GenerateHeightmap(index);
	}

	// generate the new watermap
	WatermapDownShift();
	GenerateWatermap();
}

void TerrainGenerator::WatermapLeftShift()
{
	VkOffset3D full_dimensions = { WATER_SIZE, WATER_SIZE, 1 };

	// clear the segment copy image
	VkClearColorValue clear_color = { -1, -1, -1, -1 };
	devices_->ClearColorImage(watermap_segment_image_, VK_IMAGE_LAYOUT_GENERAL, clear_color);

	// copy the segment to the cleared backup image
	VkOffset3D src_offset = { 0, 0, 0 };
	VkOffset3D dst_offset = { WATER_SIZE / TERRAIN_CHUNK_SIZE, 0, 0 };
	VkOffset3D dimensions = { WATER_SIZE - (WATER_SIZE / TERRAIN_CHUNK_SIZE), WATER_SIZE, 1 };
	devices_->CopyImage(watermap_image_, watermap_segment_image_, dimensions, src_offset, dst_offset);

	// copy the backup image back to the watermap
	devices_->CopyImage(watermap_segment_image_, watermap_image_, full_dimensions);
}

void TerrainGenerator::WatermapRightShift()
{
	VkOffset3D full_dimensions = { WATER_SIZE, WATER_SIZE, 1 };

	// clear the segment copy image
	VkClearColorValue clear_color = { -1, -1, -1, -1 };
	devices_->ClearColorImage(watermap_segment_image_, VK_IMAGE_LAYOUT_GENERAL, clear_color);

	// copy the segment to the cleared backup image
	VkOffset3D src_offset = { WATER_SIZE / TERRAIN_CHUNK_SIZE, 0, 0 };
	VkOffset3D dst_offset = { 0, 0, 0 };
	VkOffset3D dimensions = { WATER_SIZE - (WATER_SIZE / TERRAIN_CHUNK_SIZE), WATER_SIZE, 1 };
	devices_->CopyImage(watermap_image_, watermap_segment_image_, dimensions, src_offset, dst_offset);

	// copy the backup image back to the watermap
	devices_->CopyImage(watermap_segment_image_, watermap_image_, full_dimensions);
}

void TerrainGenerator::WatermapUpShift()
{
	VkOffset3D full_dimensions = { WATER_SIZE, WATER_SIZE, 1 };

	// clear the segment copy image
	VkClearColorValue clear_color = { -1, -1, -1, -1 };
	devices_->ClearColorImage(watermap_segment_image_, VK_IMAGE_LAYOUT_GENERAL, clear_color);

	// copy the segment to the cleared backup image
	VkOffset3D src_offset = { 0, 0, 0 };
	VkOffset3D dst_offset = { 0, WATER_SIZE / TERRAIN_CHUNK_SIZE, 0 };
	VkOffset3D dimensions = { WATER_SIZE, WATER_SIZE - (WATER_SIZE / TERRAIN_CHUNK_SIZE), 1 };
	devices_->CopyImage(watermap_image_, watermap_segment_image_, dimensions, src_offset, dst_offset);

	// copy the backup image back to the watermap
	devices_->CopyImage(watermap_segment_image_, watermap_image_, full_dimensions);
}

void TerrainGenerator::WatermapDownShift()
{
	VkOffset3D full_dimensions = { WATER_SIZE, WATER_SIZE, 1 };

	// clear the segment copy image
	VkClearColorValue clear_color = { -1, -1, -1, -1 };
	devices_->ClearColorImage(watermap_segment_image_, VK_IMAGE_LAYOUT_GENERAL, clear_color);

	// copy the segment to the cleared backup image
	VkOffset3D src_offset = { 0, WATER_SIZE / TERRAIN_CHUNK_SIZE, 0 };
	VkOffset3D dst_offset = { 0, 0, 0 };
	VkOffset3D dimensions = { WATER_SIZE, WATER_SIZE - (WATER_SIZE / TERRAIN_CHUNK_SIZE), 1 };
	devices_->CopyImage(watermap_image_, watermap_segment_image_, dimensions, src_offset, dst_offset);

	// copy the backup image back to the watermap
	devices_->CopyImage(watermap_segment_image_, watermap_image_, full_dimensions);
}

void TerrainGenerator::InitPipeline()
{
	// create the heightmap generation pipeline resources
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &heightmap_generation_semaphore_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}

	// create the heightmap data buffer
	generation_data_size_ = sizeof(FbmGenerationData);
	devices_->CreateBuffer(generation_data_size_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, heightmap_data_buffer_, heightmap_data_buffer_memory_);

	// create the heightmap shader
	heightmap_generation_shader_ = new VulkanComputeShader();
	heightmap_generation_shader_->Init(devices_, swap_chain_, "../res/shaders/fbm_perlin_heightmap.comp.spv");

	// initialize the heightmap_generation pipelines
	heightmap_generation_pipelines_.resize(TERRAIN_CHUNK_COUNT);
	for (int i = 0; i < TERRAIN_CHUNK_COUNT; i++)
	{
		heightmap_generation_pipelines_[i] = new HeightmapGenerationPipeline();
		heightmap_generation_pipelines_[i]->SetShader(heightmap_generation_shader_);
		heightmap_generation_pipelines_[i]->AddStorageImage(0, heightmap_image_views_[i]);
		heightmap_generation_pipelines_[i]->AddUniformBuffer(1, heightmap_data_buffer_, sizeof(generation_data_size_));
		heightmap_generation_pipelines_[i]->Init(devices_);
	}

	// create the watermap generation pipeline resources
	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &watermap_generation_sempahore_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}

	// create the watermap data buffer
	devices_->CreateBuffer(sizeof(WaterGenerationFactors), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, watermap_data_buffer_, watermap_data_buffer_memory_);

	// create the watermap shader
	watermap_generation_shader_ = new VulkanComputeShader();
	watermap_generation_shader_->Init(devices_, swap_chain_, "../res/shaders/water_map_generation.comp.spv");

	// create the watermap generation pipeline
	watermap_generation_pipeline_ = new WatermapGenerationPipeline();
	watermap_generation_pipeline_->SetShader(watermap_generation_shader_);
	watermap_generation_pipeline_->AddStorageImage(0, watermap_image_view_);
	watermap_generation_pipeline_->AddUniformBuffer(1, watermap_data_buffer_, sizeof(WaterGenerationFactors));
	watermap_generation_pipeline_->AddStorageImageArray(2, heightmap_image_views_);
	watermap_generation_pipeline_->Init(devices_);
}

void TerrainGenerator::InitResources()
{
	// create the heightmap resources
	heightmap_images_.resize(TERRAIN_CHUNK_COUNT);
	heightmap_image_memorys_.resize(TERRAIN_CHUNK_COUNT);
	heightmap_image_views_.resize(TERRAIN_CHUNK_COUNT);

	for (int i = 0; i < TERRAIN_CHUNK_COUNT; i++)
	{
		// initialize the heightmap
		devices_->CreateImage(TERRAIN_SIZE, TERRAIN_SIZE, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, heightmap_images_[i], heightmap_image_memorys_[i]);
		heightmap_image_views_[i] = devices_->CreateImageView(heightmap_images_[i], IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

		// transition to the general image layout
		devices_->TransitionImageLayout(heightmap_images_[i], IMAGE_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}

	// create the watermap resources 
	devices_->CreateImage(WATER_SIZE, WATER_SIZE, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, watermap_image_, watermap_image_memory_);
	watermap_image_view_ = devices_->CreateImageView(watermap_image_, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	devices_->TransitionImageLayout(watermap_image_, IMAGE_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	devices_->CreateImage(WATER_SIZE, WATER_SIZE, IMAGE_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, watermap_segment_image_, watermap_segment_image_memory_);
	watermap_segment_image_view_ = devices_->CreateImageView(watermap_image_, IMAGE_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
	devices_->TransitionImageLayout(watermap_segment_image_, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);


	// initially clear the water map to -1
	VkCommandBuffer clear_buffer = devices_->BeginSingleTimeCommands();

	VkImageSubresourceRange image_range = {};
	image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	VkClearColorValue clear_color = { -1.0f, -1.0f, -1.0f, -1.0f };

	vkCmdClearColorImage(clear_buffer, watermap_image_, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &image_range);

	devices_->EndSingleTimeCommands(clear_buffer);

}

void TerrainGenerator::InitCommandBuffer(VkCommandPool command_pool)
{
	// create heightmap generation command buffers
	heightmap_generation_command_buffers_.resize(TERRAIN_CHUNK_COUNT);

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = TERRAIN_CHUNK_COUNT;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, heightmap_generation_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate heightmap command buffers!");
	}

	for (int i = 0; i < TERRAIN_CHUNK_COUNT; i++)
	{

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(heightmap_generation_command_buffers_[i], &begin_info);

		heightmap_generation_pipelines_[i]->RecordCommands(heightmap_generation_command_buffers_[i]);

		if (vkEndCommandBuffer(heightmap_generation_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record heightmap command buffer!");
		}
	}

	// create the watermap generation command buffer
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;
	
	// create the watermap generation command buffer
	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &watermap_generation_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate heightmap command buffers!");
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(watermap_generation_command_buffer_, &begin_info);

	watermap_generation_pipeline_->RecordCommands(watermap_generation_command_buffer_);

	if (vkEndCommandBuffer(watermap_generation_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record watermap command buffer!");
	}
}