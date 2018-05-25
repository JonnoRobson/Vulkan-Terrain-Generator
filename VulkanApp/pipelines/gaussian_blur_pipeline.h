#ifndef _GAUSSIAN_BLUR_PIPELINE_H_
#define _GAUSSIAN_BLUR_PIPELINE_H_

#include "pipeline.h"

class GaussianBlurPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetOutputImage(VkImageView output_view, VkFormat output_format, uint32_t output_width, uint32_t output_height);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:
	VkImageView output_image_view_;
	VkFormat output_image_format_;

	uint32_t output_width_, output_height_;
};

#endif