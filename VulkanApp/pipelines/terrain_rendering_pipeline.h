#ifndef _TERRAIN_RENDERING_PIPELINE_H_
#define _TERRAIN_RENDERING_PIPELINE_H_

#include <glm\glm.hpp>
#include "pipeline.h"

struct TerrainRenderData
{
	float terrain_size;
	glm::vec3 camera_pos;
	glm::vec4 water_size;
};

struct TerrainLODFactors
{
	glm::vec4 minMaxDistance;
	glm::vec4 minMaxLOD;
	glm::vec4 cameraPos;
};

struct FogFactors
{
	float extinction_factor;
	float in_scattering_factor;
	float camera_height;
	float padding;
};

class TerrainRenderingPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateFramebuffers();
	void CreateRenderPass();
};

#endif