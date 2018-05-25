#include "watermap_generation_pipeline.h"
#include "../terrain_generator.h"

void WatermapGenerationPipeline::RecordCommands(VkCommandBuffer & command_buffer)
{
	// bind the pipeline
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);

	// bind the descriptor sets
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

	// determine workgroup counts
	uint32_t workgroup_size_x = 32;
	uint32_t workgroup_size_y = 32;

	VkExtent2D image_extent = { WATER_SIZE, WATER_SIZE };

	uint32_t workgroup_count_x = image_extent.width / workgroup_size_x;
	if (image_extent.width % workgroup_size_x > 0)
		workgroup_count_x++;

	uint32_t workgroup_count_y = image_extent.height / workgroup_size_y;
	if (image_extent.height % workgroup_size_y > 0)
		workgroup_count_y++;

	vkCmdDispatch(command_buffer, workgroup_count_x, workgroup_count_y, 1);
}
