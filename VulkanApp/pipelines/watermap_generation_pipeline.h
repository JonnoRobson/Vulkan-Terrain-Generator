#ifndef _WATERMAP_GENERATION_PIPELINE_H_
#define _WATERMAP_GENERATION_PIPELINE_H_

#include <glm\glm.hpp>
#include "compute_pipeline.h"

struct WaterGenerationFactors
{
	glm::vec2 position_offset;
	float seed;
	float size;
	float terrain_size;
	float terrain_chunk_size;
	float height_offset;
	float decay_value;
	float water_level;
	glm::vec3 padding;
};

class WatermapGenerationPipeline : public VulkanComputePipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer);
};

#endif