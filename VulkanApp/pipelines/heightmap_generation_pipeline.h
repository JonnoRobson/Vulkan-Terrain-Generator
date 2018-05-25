#ifndef _HEIGHTMAP_GENERATION_PIPELINE_H_
#define _HEIGHTMAP_GENERATION_PIPELINE_H_

#include <glm\glm.hpp>

#include "compute_pipeline.h"

struct CombinedPerlinGenerationData
{
	glm::vec2 position_offset;
	float shape_sample_width;
	float shape_intensity;
	float detail_sample_width;
	float detail_intensity;
	float interpolation_value;
	float seed;
	float terrain_size;
	float padding;
};

struct FbmGenerationData
{
	glm::vec2 position_offset;
	float octaves;
	float amplitude;
	float frequency;
	float seed;
	float filter_width;
	float size;
};

class HeightmapGenerationPipeline : public VulkanComputePipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer);

};

#endif