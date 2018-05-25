#include "renderer.h"
#include <array>
#include <map>

void VulkanRenderer::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	// load a default texture
	default_texture_ = new Texture();
	default_texture_->Init(devices, "../res/textures/default.png");

	// create the texture cache
	texture_cache_ = new VulkanTextureCache(devices);

	// load the terrain textures
	mountain_texture_ = texture_cache_->LoadTexture("../res/textures/mountain.png");
	sand_texture_ = texture_cache_->LoadTexture("../res/textures/beach_sand.png");
	grass_texture_ = texture_cache_->LoadTexture("../res/textures/grass01.png");

	current_chunk_x_ = 0;
	current_chunk_y_ = 0;

	CreateShaders();
	CreateCommandPool();
	CreateBuffers();
	CreateSemaphores();

	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue_);
	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().compute_family, 0, &compute_queue_);
}

void VulkanRenderer::RenderScene()
{
	// get swap chain index
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();
	VkExtent2D swap_extent = swap_chain_->GetSwapChainExtent();
	
	// check if the player has moved between chunks
	int old_chunk_x = current_chunk_x_;
	int old_chunk_y = current_chunk_y_;

	current_chunk_x_ = round(render_camera_->GetPosition().x / (float)TERRAIN_SIZE);
	current_chunk_y_ = round(render_camera_->GetPosition().y / (float)TERRAIN_SIZE);

	if (current_chunk_x_ > old_chunk_x)
		terrain_generator_->MoveRight(current_chunk_x_, current_chunk_y_);
	else if (current_chunk_x_ < old_chunk_x)
		terrain_generator_->MoveLeft(current_chunk_x_, current_chunk_y_);

	if (current_chunk_y_ > old_chunk_y)
		terrain_generator_->MoveUp(current_chunk_x_, current_chunk_y_);
	else if (current_chunk_y_ < old_chunk_y)
		terrain_generator_->MoveDown(current_chunk_x_, current_chunk_y_);

	// update the interpolation value based on time
	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time = std::chrono::high_resolution_clock::now();
	float time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.0f;

	bool chunk_move = (current_chunk_x_ != old_chunk_x || current_chunk_y_ != old_chunk_y);

	// only update the world matrices if the player has moved between chunks
	if (chunk_move)
	{
		for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
		{
			for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
			{
				glm::mat4 scale = glm::scale(glm::vec3(TERRAIN_SIZE / 2, TERRAIN_SIZE / 2, TERRAIN_SIZE / 2));
				glm::mat4 rot = glm::mat4(1.0f);

				int chunk_x = current_chunk_x_ - (TERRAIN_CHUNK_SIZE / 2) + x;
				int chunk_y = current_chunk_y_ + (TERRAIN_CHUNK_SIZE / 2) - y;

				glm::mat4 translate = glm::translate(glm::vec3(chunk_x * (TERRAIN_SIZE), chunk_y * (TERRAIN_SIZE), 0.0f));
				glm::mat4 world = translate * rot * scale;
				terrain_matrix_data_.world[x + (y * TERRAIN_CHUNK_SIZE)] = world;
			}
		}
	}
	// update the matrix buffer
	terrain_matrix_data_.view = render_camera_->GetViewMatrix();
	terrain_matrix_data_.proj = render_camera_->GetProjectionMatrix();
	devices_->CopyDataToBuffer(terrain_matrix_buffer_memory_, &terrain_matrix_data_, sizeof(TerrainMatrixData));

	// update the lod factors buffer
	TerrainLODFactors lod = {};
	lod.minMaxDistance = glm::vec4(4.0f, 1000.0f, 0.0f, 0.0f);
	lod.minMaxLOD = glm::vec4(1.0f, 10.0f, 0.0f, 0.0f);
	lod.cameraPos = glm::vec4(render_camera_->GetPosition(), 1.0f);
	devices_->CopyDataToBuffer(terrain_lod_factors_buffer_memory_, &lod, sizeof(TerrainLODFactors));

	// update the terrain render data buffer
	TerrainRenderData data = {};
	data.terrain_size = TERRAIN_SIZE;
	data.camera_pos = render_camera_->GetPosition();
	data.water_size = glm::vec4(WATER_SIZE);
	devices_->CopyDataToBuffer(terrain_render_data_buffer_memory_, &data, sizeof(TerrainRenderData));

	// update the water matrix buffer
	WaterMatrixData water_matrices = {}; 
	glm::mat4 scale = glm::scale(glm::vec3((TERRAIN_SIZE / 2) * TERRAIN_CHUNK_SIZE, (TERRAIN_SIZE / 2) * TERRAIN_CHUNK_SIZE, TERRAIN_SIZE / 2));
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 translate = glm::translate(glm::vec3(current_chunk_x_ * TERRAIN_SIZE, current_chunk_y_ * TERRAIN_SIZE, 0.0f));
	glm::mat4 world = translate * rot * scale;
	water_matrices.world = world;
	water_matrices.view = render_camera_->GetViewMatrix();
	water_matrices.proj = render_camera_->GetProjectionMatrix();
	devices_->CopyDataToBuffer(water_matrix_buffer_memory_, &water_matrices, sizeof(WaterMatrixData));

	// update the water render data buffer
	WaterRenderData water_data = {};
	water_data.camera_pos = render_camera_->GetPosition();
	water_data.water_size = WATER_SIZE;
	water_data.time = time_gap;
	water_data.padding[0] = 0;
	water_data.padding[1] = 1;
	water_data.padding[2] = 2;
	devices_->CopyDataToBuffer(water_render_data_buffer_memory_, &water_data, sizeof(WaterRenderData));

	// update fog factors buffer
	FogFactors fog_factors = {};
	fog_factors.extinction_factor = 0.004f;
	fog_factors.in_scattering_factor = 0.002f;
	fog_factors.camera_height = render_camera_->GetPosition().z;
	fog_factors.padding = 0;
	devices_->CopyDataToBuffer(fog_factors_buffer_memory_, &fog_factors, sizeof(FogFactors));
	
	// terrain rendering
	// render the skybox
	skybox_->Render(render_camera_);

	// render the terrain
	RenderTerrain();
	render_semaphore_ = terrain_rendering_semaphore_;
	
	// render the water
	RenderWater();
	render_semaphore_ = water_rendering_semaphore_;

	if (hdr_->GetHDRMode() > 0)
	{
		hdr_->Render(swap_chain_, &render_semaphore_);
		render_semaphore_ = hdr_->GetHDRSemaphore();
	}
	
	// render the watermap
	//RenderVisualisation();
	//render_semaphore_ = visualisation_semaphore_;

	swap_chain_->FinalizeIntermediateImage();
}

void VulkanRenderer::RenderVisualisation()
{
	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &buffer_visualisation_command_buffer_;

	VkSemaphore signal_semaphores[] = { visualisation_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;
	
	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit visualisation command buffer!");
	}
}

void VulkanRenderer::RenderTerrain()
{
	// determine which terrain chunks to render
	std::vector<VkCommandBuffer> visible_chunk_commands = CheckTerrainVisibility();

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { skybox_->GetRenderSemaphore() };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = visible_chunk_commands.size();
	submit_info.pCommandBuffers = visible_chunk_commands.data();

	VkSemaphore signal_semaphores[] = { terrain_rendering_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit terrain render command buffer!");
	}
}

void VulkanRenderer::RenderWater()
{
	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { terrain_rendering_semaphore_ };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &water_rendering_command_buffer_;

	VkSemaphore signal_semaphores[] = { water_rendering_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit water render command buffer!");
	}
}

std::vector<VkCommandBuffer> VulkanRenderer::CheckTerrainVisibility()
{
	std::vector<VkCommandBuffer> visible_chunk_commands;
	
	// player can always see the center chunk
	int center_index = (TERRAIN_CHUNK_COUNT - 1) / 2;
	visible_chunk_commands.push_back(terrain_rendering_command_buffers_[center_index]);

	// determine the position in the center chunk
	float int_part;
	glm::vec3 camera_pos = render_camera_->GetPosition();
	camera_pos.z = 0;
	camera_pos = camera_pos / (float)TERRAIN_SIZE;
	camera_pos = glm::vec3(modf(camera_pos.x, &int_part), modf(camera_pos.y, &int_part), modf(camera_pos.z, &int_part));

	// determine the edges of the camera frustum
	glm::vec3 camera_rot = render_camera_->GetRotation();
	glm::vec3 look_at = glm::rotateY(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(camera_rot.y));
	look_at = glm::rotateZ(look_at, glm::radians(camera_rot.z));
	look_at = look_at + camera_pos;
	glm::vec3 view_left_edge = glm::rotateZ(look_at, glm::radians(-60.0f));
	glm::vec3 view_right_edge = glm::rotateZ(look_at, glm::radians(60.0f));

	glm::vec3 v0 = camera_pos;
	glm::vec3 v1 = view_left_edge * (float)TERRAIN_CHUNK_SIZE;
	glm::vec3 v2 = view_right_edge * (float)TERRAIN_CHUNK_SIZE;

	// compute the view area
	glm::vec3 v0v1 = v1 - v0;
	glm::vec3 v0v2 = v2 - v0;
	glm::vec3 n = glm::cross(v0v1, v0v2);
	float area = n.length() / 2;

	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
		{
			int i = x + (y * TERRAIN_CHUNK_SIZE);

			if (i == center_index)
				continue;

			// determine the four corner points of the chunk
			float chunk_center_x = ((TERRAIN_CHUNK_SIZE / 2) * -1) + x;
			float chunk_center_y = (TERRAIN_CHUNK_SIZE / 2) - y;
			glm::vec3 chunk_center_pos = glm::vec3(chunk_center_x, chunk_center_y, 0.0);

			glm::vec3 chunk_top_left = chunk_center_pos + glm::vec3(-0.5f, 0.5f, 0.0f);
			glm::vec3 chunk_top_right = chunk_center_pos + glm::vec3(0.5f, 0.5f, 0.0f);
			glm::vec3 chunk_bot_left = chunk_center_pos + glm::vec3(-0.5f, -0.5f, 0.0f);
			glm::vec3 chunk_bot_right = chunk_center_pos + glm::vec3(0.5f, -0.5f, 0.0f);

			// determine barycentric coordinates for each of the corner points to see if they fall inside the frustum
			glm::vec3 c0 = glm::vec3(0, 0, 0);
			glm::vec3 c1 = glm::vec3(0, 0, 0);
			glm::vec3 vp1 = glm::vec3(0, 0, 0);
			glm::vec3 vp2 = glm::vec3(0, 0, 0);

			// top left
			vp1 = chunk_top_left - v1;
			c0 = glm::cross(view_left_edge, vp1);
			vp2 = chunk_top_left - v2;
			c1 = glm::cross(view_right_edge, vp2);
			if (glm::dot(n, c0) >= 0 && glm::dot(n, c1) < 0)
			{
				// chunk falls within the view frustum
				visible_chunk_commands.push_back(terrain_rendering_command_buffers_[i]);
				continue;
			}

			// top right
			vp1 = chunk_top_right - v1;
			c0 = glm::cross(view_left_edge, vp1);
			vp2 = chunk_top_right - v2;
			c1 = glm::cross(view_right_edge, vp2);
			if (glm::dot(n, c0) >= 0 && glm::dot(n, c1) < 0)
			{
				// chunk falls within the view frustum
				visible_chunk_commands.push_back(terrain_rendering_command_buffers_[i]);
				continue;
			}

			// top left
			vp1 = chunk_bot_left - v1;
			c0 = glm::cross(view_left_edge, vp1);
			vp2 = chunk_bot_left - v2;
			c1 = glm::cross(view_right_edge, vp2);
			if (glm::dot(n, c0) >= 0 && glm::dot(n, c1) < 0)
			{
				// chunk falls within the view frustum
				visible_chunk_commands.push_back(terrain_rendering_command_buffers_[i]);
				continue;
			}

			// top left
			vp1 = chunk_bot_right - v1;
			c0 = glm::cross(view_left_edge, vp1);
			vp2 = chunk_bot_right - v2;
			c1 = glm::cross(view_right_edge, vp2);
			if (glm::dot(n, c0) >= 0 && glm::dot(n, c1) < 0)
			{
				// chunk falls within the view frustum
				visible_chunk_commands.push_back(terrain_rendering_command_buffers_[i]);
				continue;
			}
		}
	}

	return visible_chunk_commands;
}

void VulkanRenderer::Cleanup()
{
	// clean up command and descriptor pools
	vkDestroyCommandPool(devices_->GetLogicalDevice(), command_pool_, nullptr);

	// clean up buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), terrain_matrix_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), terrain_matrix_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), terrain_lod_factors_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), terrain_lod_factors_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), terrain_render_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), terrain_render_data_buffer_memory_, nullptr);
	
	vkDestroyBuffer(devices_->GetLogicalDevice(), fog_factors_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), fog_factors_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), water_matrix_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), water_matrix_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), water_render_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), water_render_data_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), color_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), color_data_buffer_memory_, nullptr);

	// clean up shaders
	buffer_visualisation_shader_->Cleanup();
	delete buffer_visualisation_shader_;
	buffer_visualisation_shader_ = nullptr;
	
	terrain_shader_->Cleanup();
	delete terrain_shader_;
	terrain_shader_ = nullptr;

	water_shader_->Cleanup();
	delete water_shader_;
	water_shader_ = nullptr;

	// clean up the texture cache
	texture_cache_->Cleanup();
	delete texture_cache_;
	texture_cache_ = nullptr;

	// clean up the buffer visualisation pipeline
	buffer_visualisation_pipeline_->CleanUp();
	delete buffer_visualisation_pipeline_;
	buffer_visualisation_pipeline_ = nullptr;

	// clean up terrain rendering pipeline
	terrain_rendering_pipeline_->CleanUp();
	delete terrain_rendering_pipeline_;
	terrain_rendering_pipeline_ = nullptr;

	// clean up water rendering pipeline
	water_rendering_pipeline_->CleanUp();
	delete water_rendering_pipeline_;
	water_rendering_pipeline_ = nullptr;

	// clean up the skybox
	skybox_->Cleanup();
	delete skybox_;
	skybox_ = nullptr;

	// clean up the hdr renderer
	hdr_->Cleanup();
	delete hdr_;
	hdr_ = nullptr;

	// clean up terrain mesh
	delete terrain_mesh_;
	terrain_mesh_ = nullptr;

	// clean up water mesh
	delete water_mesh_;
	water_mesh_ = nullptr;

	// clean up the texture generator
	terrain_generator_->Cleanup();
	delete terrain_generator_;
	terrain_generator_ = nullptr;

	// clean up default texture
	default_texture_->Cleanup();
	delete default_texture_;
	default_texture_ = nullptr;

	// clean up samplers
	vkDestroySampler(devices_->GetLogicalDevice(), buffer_normalized_sampler_, nullptr);
	vkDestroySampler(devices_->GetLogicalDevice(), buffer_unnormalized_sampler_, nullptr);

	// clean up semaphores
	vkDestroySemaphore(devices_->GetLogicalDevice(), visualisation_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), terrain_rendering_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), water_rendering_semaphore_, nullptr);
}

void VulkanRenderer::InitPipelines()
{
	// initialize the skybox
	skybox_ = new Skybox();
	skybox_->Init(devices_, swap_chain_, command_pool_, color_data_buffer_);

	// initialize the hdr renderer
	hdr_ = new HDR();
	hdr_->Init(devices_, swap_chain_, command_pool_);

	// initialize the terrain mesh
	terrain_mesh_ = new Mesh();
	terrain_mesh_->CreateTerrainMesh(devices_, TERRAIN_SIZE);

	// initialize the water mesh
	water_mesh_ = new Mesh();
	water_mesh_->CreateTerrainMesh(devices_, TERRAIN_SIZE);

	// initialize the terrain generator
	terrain_generator_ = new TerrainGenerator();
	terrain_generator_->Init(devices_, swap_chain_, command_pool_, compute_queue_);

	// generate initial heightmaps
	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
		{
			int i = x + (y * TERRAIN_CHUNK_SIZE);

			int current_chunk_x = (x - (TERRAIN_CHUNK_SIZE / 2));
			int current_chunk_y = ((TERRAIN_CHUNK_SIZE - 1 - y) - (TERRAIN_CHUNK_SIZE / 2));

			float offset_x = current_chunk_x * TERRAIN_SIZE;
			float offset_y = current_chunk_y * TERRAIN_SIZE;

			terrain_generator_->SetPositionOffset(glm::vec2(offset_x, offset_y));
			terrain_generator_->GenerateHeightmap(i);
		}
	}

	// generate water map
	terrain_generator_->GenerateWatermap();

	// generate initial matrices to save time while rendering
	for (int y = 0; y < TERRAIN_CHUNK_SIZE; y++)
	{
		for (int x = 0; x < TERRAIN_CHUNK_SIZE; x++)
		{
			glm::mat4 scale = glm::scale(glm::vec3(TERRAIN_SIZE / 2, TERRAIN_SIZE / 2, TERRAIN_SIZE / 2));
			glm::mat4 rot = glm::mat4(1.0f);

			int chunk_x = current_chunk_x_ - (TERRAIN_CHUNK_SIZE / 2) + x;
			int chunk_y = current_chunk_y_ + (TERRAIN_CHUNK_SIZE / 2) - y;

			glm::mat4 translate = glm::translate(glm::vec3(chunk_x * (TERRAIN_SIZE), chunk_y * (TERRAIN_SIZE), 0.0f));
			glm::mat4 world = translate * rot * scale;
			terrain_matrix_data_.world[x + (y * TERRAIN_CHUNK_SIZE)] = world;
		}
	}

	// initialize the buffer samplers
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler_info.unnormalizedCoordinates = VK_TRUE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &buffer_unnormalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create g buffer sampler!");
	}

	// initialize the shadow map sampler
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &shadow_map_sampler_) != VK_SUCCESS ||
		vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &buffer_normalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create normalized samplers!");
	}

	// initialize a buffer visualisation pipeline
	buffer_visualisation_pipeline_ = new BufferVisualisationPipeline();
	buffer_visualisation_pipeline_->SetShader(buffer_visualisation_shader_);
	buffer_visualisation_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0, default_texture_->GetSampler());
	buffer_visualisation_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 1, terrain_generator_->GetWatermap());

	buffer_visualisation_pipeline_->Init(devices_, swap_chain_);

	// create the terrain rendering pipeline
	terrain_rendering_pipeline_ = new TerrainRenderingPipeline();
	terrain_rendering_pipeline_->SetShader(terrain_shader_);
	terrain_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, terrain_matrix_buffer_, sizeof(TerrainMatrixData));
	terrain_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 1, terrain_render_data_buffer_, sizeof(TerrainRenderData));
	terrain_rendering_pipeline_->AddSampler(VK_SHADER_STAGE_VERTEX_BIT, 2, buffer_normalized_sampler_);
	terrain_rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_VERTEX_BIT, 3, terrain_generator_->GetHeightmaps());
	terrain_rendering_pipeline_->AddTexture(VK_SHADER_STAGE_VERTEX_BIT, 4, terrain_generator_->GetWatermap());
	terrain_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 5, fog_factors_buffer_, sizeof(FogFactors));
	terrain_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 6, color_data_buffer_, sizeof(ColorDataBuffer));
	terrain_rendering_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 7, mountain_texture_);
	terrain_rendering_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 8, sand_texture_);
	terrain_rendering_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 9, grass_texture_);
	terrain_rendering_pipeline_->Init(devices_, swap_chain_);

	// create the water rendering pipeline
	water_rendering_pipeline_ = new WaterRenderingPipeline();
	water_rendering_pipeline_->SetShader(water_shader_);
	water_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, water_matrix_buffer_, sizeof(WaterMatrixData));
	water_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 1, water_render_data_buffer_, sizeof(WaterRenderData));
	water_rendering_pipeline_->AddStorageImage(VK_SHADER_STAGE_VERTEX_BIT, 2, terrain_generator_->GetWatermap());
	water_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, fog_factors_buffer_, sizeof(FogFactors));
	water_rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 4, color_data_buffer_, sizeof(ColorDataBuffer));
	water_rendering_pipeline_->Init(devices_, swap_chain_);

	// generate the color data
	ColorDataBuffer color_data =
	{
		glm::vec4(1.0),
		glm::vec4(1.0),
		glm::vec4(0.65),
		glm::vec4(0.5, 0.5, 0.6, 1.0)
	};
	/*
	color_data.rockColor.x = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.rockColor.y = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.rockColor.z = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.snowColor.x = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.snowColor.y = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.snowColor.z = ((float)rand() / (float)RAND_MAX) / 2.0 + 0.75;
	color_data.fogColor.x = ((float)rand() / (float)RAND_MAX) * 0.75;
	color_data.fogColor.y = ((float)rand() / (float)RAND_MAX) * 0.75;
	color_data.fogColor.z = ((float)rand() / (float)RAND_MAX) * 0.75;
	color_data.waterColor.x = ((float)rand() / (float)RAND_MAX) * 1.0;
	color_data.waterColor.y = ((float)rand() / (float)RAND_MAX) * 1.0;
	color_data.waterColor.z = ((float)rand() / (float)RAND_MAX) * 1.0;
	*/
	devices_->CopyDataToBuffer(color_data_buffer_memory_, &color_data, sizeof(ColorDataBuffer));

	CreateCommandBuffers();
}

void VulkanRenderer::RecreateSwapChainFeatures()
{
}

void VulkanRenderer::CreateCommandPool()
{
	QueueFamilyIndices queue_family_indices = VulkanDevices::FindQueueFamilies(devices_->GetPhysicalDevice(), swap_chain_->GetSurface());

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
	pool_info.flags = 0;

	if (vkCreateCommandPool(devices_->GetLogicalDevice(), &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanRenderer::CreateCommandBuffers()
{
	CreateBufferVisualisationCommandBuffers();
	CreateTerrainRenderingCommandBuffers();
	CreateWaterRenderingCommandBuffers();
}

void VulkanRenderer::CreateBufferVisualisationCommandBuffers()
{
	// create buffer visualisation command buffers
	devices_->CreateCommandBuffers(command_pool_, &buffer_visualisation_command_buffer_);

	// record the command buffers
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(buffer_visualisation_command_buffer_, &begin_info);

	if (buffer_visualisation_pipeline_)
	{
		// bind pipeline
		buffer_visualisation_pipeline_->RecordCommands(buffer_visualisation_command_buffer_, 0);

		vkCmdEndRenderPass(buffer_visualisation_command_buffer_);
	}

	if (vkEndCommandBuffer(buffer_visualisation_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record render command buffer!");
	}
}

void VulkanRenderer::CreateTerrainRenderingCommandBuffers()
{
	terrain_rendering_command_buffers_.resize(TERRAIN_CHUNK_COUNT);

	// create render command buffers for each chunk
	devices_->CreateCommandBuffers(command_pool_, terrain_rendering_command_buffers_.data(), TERRAIN_CHUNK_COUNT);

	// record the command buffers
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	// record the command buffer for each chunk
	for (int i = 0; i < TERRAIN_CHUNK_COUNT; i++)
	{
		vkBeginCommandBuffer(terrain_rendering_command_buffers_[i], &begin_info);

		if (terrain_rendering_pipeline_)
		{
			// bind pipeline
			terrain_rendering_pipeline_->RecordCommands(terrain_rendering_command_buffers_[i], 0);

			// render terrain mesh
			terrain_mesh_->RecordTerrainRenderCommands(terrain_rendering_command_buffers_[i], i);

			vkCmdEndRenderPass(terrain_rendering_command_buffers_[i]);
		}

		if (vkEndCommandBuffer(terrain_rendering_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}
}

void VulkanRenderer::CreateWaterRenderingCommandBuffers()
{
	// create the render command buffer
	devices_->CreateCommandBuffers(command_pool_, &water_rendering_command_buffer_);

	// record the command buffers
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	// record the water rendering commands
	vkBeginCommandBuffer(water_rendering_command_buffer_, &begin_info);

	if (water_rendering_pipeline_)
	{
		// bind pipeline
		water_rendering_pipeline_->RecordCommands(water_rendering_command_buffer_, 0);

		// render the water mesh
		water_mesh_->RecordTerrainRenderCommands(water_rendering_command_buffer_, 0);

		vkCmdEndRenderPass(water_rendering_command_buffer_);
	}

	if (vkEndCommandBuffer(water_rendering_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record render command buffer!");
	}
}

void VulkanRenderer::CreateShaders()
{
	buffer_visualisation_shader_ = new VulkanShader();
	buffer_visualisation_shader_->Init(devices_, swap_chain_, "../res/shaders/buffer_visualisation.vert.spv", "", "", "", "../res/shaders/buffer_visualisation.frag.spv");

	terrain_shader_ = new VulkanShader();
	terrain_shader_->Init(devices_, swap_chain_, "../res/shaders/terrain.vert.spv", "", "", "", "../res/shaders/terrain.frag.spv");

	water_shader_ = new VulkanShader();
	water_shader_->Init(devices_, swap_chain_, "../res/shaders/water_render.vert.spv", "", "", "", "../res/shaders/water_render.frag.spv");
}

void VulkanRenderer::CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &terrain_rendering_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &water_rendering_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &visualisation_semaphore_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}

void VulkanRenderer::CreateBuffers()
{
	// create the terrain rendering buffers
	devices_->CreateBuffer(sizeof(TerrainMatrixData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, terrain_matrix_buffer_, terrain_matrix_buffer_memory_);
	devices_->CreateBuffer(sizeof(TerrainLODFactors), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, terrain_lod_factors_buffer_, terrain_lod_factors_buffer_memory_);
	devices_->CreateBuffer(sizeof(TerrainRenderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, terrain_render_data_buffer_, terrain_render_data_buffer_memory_);
	devices_->CreateBuffer(sizeof(FogFactors), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, fog_factors_buffer_, fog_factors_buffer_memory_);
	devices_->CreateBuffer(sizeof(WaterMatrixData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, water_matrix_buffer_, water_matrix_buffer_memory_);
	devices_->CreateBuffer(sizeof(WaterRenderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, water_render_data_buffer_, water_render_data_buffer_memory_);
	devices_->CreateBuffer(sizeof(ColorDataBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, color_data_buffer_, color_data_buffer_memory_);
}