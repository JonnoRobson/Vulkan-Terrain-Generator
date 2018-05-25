#ifndef _BUFFER_VISUALISATION_PIPELINE_H_
#define _BUFFER_VISUALISATION_PIPELINE_H_

#include "pipeline.h"

class BufferVisualisationPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateFramebuffers();
	void CreateRenderPass();

};

#endif