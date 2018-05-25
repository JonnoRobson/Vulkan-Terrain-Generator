#include "shader.h"
#include "mesh.h"

void VulkanShader::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string tcs_filename, std::string tes_filename, std::string gs_filename, std::string fs_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	LoadShaders(vs_filename, tcs_filename, tes_filename, gs_filename, fs_filename);

	// create shader descriptors
	CreateVertexInput();
	CreateInputAssembly();
	CreateViewportState();
	CreateRasterizerState();
	CreateMultisampleState();
	CreateDepthStencilState();
	CreateBlendState();
	CreateDynamicState();
}

void VulkanShader::Cleanup()
{
	VkDevice device = devices_->GetLogicalDevice();

	// clean up shader modules
	vkDestroyShaderModule(device, vertex_shader_module_, nullptr);
	vkDestroyShaderModule(device, tessellation_control_shader_module_, nullptr);
	vkDestroyShaderModule(device, tessellation_evaluation_shader_module_, nullptr);
	vkDestroyShaderModule(device, geometry_shader_module_, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module_, nullptr);
}

void VulkanShader::LoadShaders(std::string vs_filename, std::string tcs_filename, std::string tes_filename, std::string gs_filename, std::string fs_filename)
{

	if (!vs_filename.empty())
	{
		auto vert_shader_code = VulkanDevices::ReadFile(vs_filename);
		vertex_shader_module_ = CreateShaderModule(vert_shader_code);

		VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
		vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertex_shader_stage_info.module = vertex_shader_module_;
		vertex_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(vertex_shader_stage_info);
	}

	if (!tcs_filename.empty())
	{
		// load tesselation control shader
		auto tessellation_control_shader_code = VulkanDevices::ReadFile(tcs_filename);
		tessellation_control_shader_module_ = CreateShaderModule(tessellation_control_shader_code);

		VkPipelineShaderStageCreateInfo tessellation_control_shader_stage_info = {};
		tessellation_control_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		tessellation_control_shader_stage_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		tessellation_control_shader_stage_info.module = tessellation_control_shader_module_;
		tessellation_control_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(tessellation_control_shader_stage_info);
	}

	if(!tes_filename.empty())
	{
		// load tessellation evaluation shader
		auto tessellation_evaluation_shader_code = VulkanDevices::ReadFile(tes_filename);
		tessellation_evaluation_shader_module_ = CreateShaderModule(tessellation_evaluation_shader_code);

		VkPipelineShaderStageCreateInfo tessellation_evaluation_shader_stage_info = {};
		tessellation_evaluation_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		tessellation_evaluation_shader_stage_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		tessellation_evaluation_shader_stage_info.module = tessellation_evaluation_shader_module_;
		tessellation_evaluation_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(tessellation_evaluation_shader_stage_info);

	}

	if (!gs_filename.empty())
	{
		auto geometry_shader_code = VulkanDevices::ReadFile(gs_filename);
		geometry_shader_module_ = CreateShaderModule(geometry_shader_code);

		VkPipelineShaderStageCreateInfo geometry_shader_stage_info = {};
		geometry_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		geometry_shader_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		geometry_shader_stage_info.module = geometry_shader_module_;
		geometry_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(geometry_shader_stage_info);
	}

	if (!fs_filename.empty())
	{
		auto frag_shader_code = VulkanDevices::ReadFile(fs_filename);
		fragment_shader_module_ = CreateShaderModule(frag_shader_code);

		VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
		frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		frag_shader_stage_info.module = fragment_shader_module_;
		frag_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(frag_shader_stage_info);
	}
}

VkShaderModule VulkanShader::CreateShaderModule(const std::vector<char>& code)
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

void VulkanShader::CreateVertexInput()
{
	CreateVertexBinding();
	CreateVertexAttributes();

	vertex_input_ = {};
	vertex_input_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_.vertexBindingDescriptionCount = 1;
	vertex_input_.pVertexBindingDescriptions = &vertex_binding_;
	vertex_input_.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes_.size());
	vertex_input_.pVertexAttributeDescriptions = vertex_attributes_.data();
}

void VulkanShader::CreateVertexBinding()
{
	// get the binding description from Vertex since this is what is used in a base render shader
	vertex_binding_ = Vertex::GetBindingDescription();
}

void VulkanShader::CreateVertexAttributes()
{
	// get the binding attributes from Vertex since this is what is used in a base render shader
	auto attribute_descriptions = Vertex::GetAttributeDescriptions();

	for (VkVertexInputAttributeDescription& description : attribute_descriptions)
	{
		vertex_attributes_.push_back(description);
	}
}

void VulkanShader::CreateInputAssembly()
{
	// setup input assembly creation info
	input_assembly_ = {};
	input_assembly_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_.primitiveRestartEnable = VK_FALSE;
}

void VulkanShader::CreateViewportState()
{
	// setup viewport creation info
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(swap_chain_->GetSwapChainExtent().width);
	viewport.height = (float)(swap_chain_->GetSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewports_.push_back(viewport);

	// setup scissor rect info
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetSwapChainExtent();
	scissor_rects_.push_back(scissor);

	// setup viewport state creation info
	viewport_state_ = {};
	viewport_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_.viewportCount = static_cast<uint32_t>(viewports_.size());
	viewport_state_.pViewports = viewports_.data();
	viewport_state_.scissorCount = static_cast<uint32_t>(scissor_rects_.size());
	viewport_state_.pScissors = scissor_rects_.data();
}

void VulkanShader::CreateRasterizerState()
{
	// setup rasterizer creation info
	rasterizer_state_ = {};
	rasterizer_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state_.depthClampEnable = VK_FALSE;
	rasterizer_state_.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state_.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state_.lineWidth = 1.0f;
	rasterizer_state_.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_state_.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_state_.depthBiasEnable = VK_FALSE;
	rasterizer_state_.depthBiasConstantFactor = 0.0f;
	rasterizer_state_.depthBiasClamp = 0.0f;
	rasterizer_state_.depthBiasSlopeFactor = 0.0f;
}

void VulkanShader::CreateMultisampleState()
{
	// setup multisample state creation info
	multisample_state_= {};
	multisample_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state_.sampleShadingEnable = VK_FALSE;
	multisample_state_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state_.minSampleShading = 1.0f;
	multisample_state_.pSampleMask = nullptr;
	multisample_state_.alphaToCoverageEnable = VK_FALSE;
	multisample_state_.alphaToOneEnable = VK_FALSE;
}

void VulkanShader::CreateDepthStencilState()
{
	// setup the depth stencil state creation info
	depth_stencil_state_ = {};
	depth_stencil_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state_.depthTestEnable = VK_TRUE;
	depth_stencil_state_.depthWriteEnable = VK_TRUE;
	depth_stencil_state_.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state_.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state_.minDepthBounds = 0.0f;
	depth_stencil_state_.maxDepthBounds = 1.0f;
	depth_stencil_state_.stencilTestEnable = VK_FALSE;
	depth_stencil_state_.front = {};
	depth_stencil_state_.back = {};
}

void VulkanShader::CreateBlendState()
{
	// setup color blend creation info
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_states_.push_back(color_blend_attachment);

	// setup global color blend creation info
	blend_state_ = {};
	blend_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state_.logicOpEnable = VK_FALSE;
	blend_state_.logicOp = VK_LOGIC_OP_COPY;
	blend_state_.attachmentCount = static_cast<uint32_t>(attachment_blend_states_.size());
	blend_state_.pAttachments = attachment_blend_states_.data();
	blend_state_.blendConstants[0] = 0.0f;
	blend_state_.blendConstants[1] = 0.0f;
	blend_state_.blendConstants[2] = 0.0f;
	blend_state_.blendConstants[3] = 0.0f;
}

void VulkanShader::CreateDynamicState()
{
	// setup pipeline dynamic state info
	dynamic_states_.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamic_states_.push_back(VK_DYNAMIC_STATE_SCISSOR);

	dynamic_state_description_ = {};
	dynamic_state_description_.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_description_.dynamicStateCount = static_cast<uint32_t>(dynamic_states_.size());
	dynamic_state_description_.pDynamicStates = dynamic_states_.data();
}