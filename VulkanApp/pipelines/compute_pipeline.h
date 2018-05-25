#ifndef _COMPUTE_PIPELINE_H_
#define _COMPUTE_PIPELINE_H_

#include "../device.h"
#include "../compute_shader.h"
#include "../texture.h"

class VulkanComputePipeline
{
protected:
	struct Descriptor
	{
		std::vector<VkDescriptorBufferInfo> buffer_infos;
		std::vector<VkDescriptorImageInfo> image_infos;
		VkDescriptorSetLayoutBinding layout_binding;
	};

public:
	void Init(VulkanDevices* devices);
	void CleanUp();

	inline void SetShader(VulkanComputeShader* shader) { shader_ = shader; }

	void AddTexture(uint32_t binding_location, Texture* texture);
	void AddTexture(uint32_t binding_location, VkImageView image);
	void AddTextureArray(uint32_t binding_location, std::vector<Texture*>& textures);
	void AddTextureArray(uint32_t binding_location, std::vector<VkImageView>& textures);
	void AddSampler(uint32_t binding_location, VkSampler sampler);
	void AddUniformBuffer(uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);
	void AddStorageBuffer(uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);
	void AddStorageImage(uint32_t binding_location, VkImageView image);
	void AddStorageImageArray(uint32_t binding_location, std::vector<VkImageView>& images);

	virtual void RecordCommands(VkCommandBuffer& command_buffer) = 0;

protected:
	void CreateDescriptorSet();
	virtual void CreatePipeline();

protected:
	VulkanDevices* devices_;

	VulkanComputeShader* shader_;

	VkPipelineLayout pipeline_layout_;
	VkPipeline pipeline_;

	VkDescriptorPool descriptor_pool_;
	VkDescriptorSetLayout descriptor_set_layout_;
	VkDescriptorSet descriptor_set_;
	std::vector<Descriptor> descriptor_infos_;	// info used in the creation of the pipeline
};
#endif