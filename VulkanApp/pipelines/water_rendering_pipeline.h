#ifndef _WATER_RENDERING_PIPELINE_H_
#define _WATER_RENDERING_PIPELINE_H_

#include "pipeline.h"
#include <glm/glm.hpp>

struct WaterMatrixData
{
	glm::mat4 world;
	glm::mat4 view;
	glm::mat4 proj;
};

struct WaterRenderData
{
	glm::vec3 camera_pos;
	float water_size;
	float time;
	float padding[3];
};


class WaterRenderingPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateFramebuffers();
	void CreateRenderPass();
};

#endif