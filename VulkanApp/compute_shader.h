#ifndef _COMPUTE_SHADER_H_
#define _COMPUTE_SHADER_H_

#include "device.h"
#include"swap_chain.h"

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanComputeShader
{
public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string cs_filename);
	virtual void Cleanup();

	// getters
	inline VkShaderModule GetComputeShader() { return compute_shader_module_; }
	inline VkPipelineShaderStageCreateInfo GetShaderStageInfo() { return shader_stage_info_; }

protected:
	virtual void LoadShader(std::string cs_filename);

	VkShaderModule CreateShaderModule(const std::vector<char>& code);

protected:
	// device and swap chain handles for function calls
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;

	// pipeline creation data
	VkPipelineShaderStageCreateInfo shader_stage_info_;
	VkShaderModule compute_shader_module_;
};

#endif