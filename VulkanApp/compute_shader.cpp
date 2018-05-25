#include "compute_shader.h"


void VulkanComputeShader::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string cs_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	LoadShader(cs_filename);
}

void VulkanComputeShader::Cleanup()
{
	VkDevice device = devices_->GetLogicalDevice();

	// clean up shader modules
	vkDestroyShaderModule(device, compute_shader_module_, nullptr);
}

void VulkanComputeShader::LoadShader(std::string cs_filename)
{

	if (!cs_filename.empty())
	{
		auto comp_shader_code = VulkanDevices::ReadFile(cs_filename);
		compute_shader_module_ = CreateShaderModule(comp_shader_code);

		shader_stage_info_ = {};
		shader_stage_info_.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_info_.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shader_stage_info_.module = compute_shader_module_;
		shader_stage_info_.pName = "main";
		
	}
}

VkShaderModule VulkanComputeShader::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(devices_->GetLogicalDevice(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shader_module;
}