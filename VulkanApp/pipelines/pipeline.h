#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include "../device.h"
#include "../swap_chain.h"
#include "../texture.h"
#include "../shader.h"

class VulkanPipeline
{
protected:
	struct Descriptor
	{
		std::vector<VkDescriptorBufferInfo> buffer_infos;
		std::vector<VkDescriptorImageInfo> image_infos;
		VkDescriptorSetLayoutBinding layout_binding;
	};

public:
	VulkanPipeline();
	
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain);
	void CleanUp();
	void RecreateSwapChainFeatures();

	inline void SetShader(VulkanShader* shader) { shader_ = shader; }

	void AddTexture(VkShaderStageFlags stage_flags, uint32_t binding_location, Texture* texture);
	void AddTexture(VkShaderStageFlags stage_flags, uint32_t binding_location, VkImageView image);
	void AddTextureArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<Texture*>& textures);
	void AddTextureArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<VkImageView>& textures);
	void AddSampler(VkShaderStageFlags stage_flags, uint32_t binding_location, VkSampler sampler);
	void AddUniformBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);
	void AddStorageBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);
	void AddStorageImage(VkShaderStageFlags stage_flags, uint32_t binding_location, VkImageView image);
	void AddStorageImageArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<VkImageView>& images);
	virtual void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:

	void CreateDescriptorSet();
	virtual void CreateRenderPass();
	virtual void CreateFramebuffers();
	virtual void CreatePipeline();

protected:

	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;

	VulkanShader* shader_;

	VkRenderPass render_pass_;
	std::vector<VkFramebuffer> framebuffers_;
	VkPipelineLayout pipeline_layout_;
	VkPipeline pipeline_;


	VkDescriptorPool descriptor_pool_;
	VkDescriptorSetLayout descriptor_set_layout_;
	VkDescriptorSet descriptor_set_;

	// info used in the creation of the pipeline
	std::vector<Descriptor> descriptor_infos_;
};

#endif