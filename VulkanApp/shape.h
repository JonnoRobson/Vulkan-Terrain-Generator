#ifndef _SHAPE_H_
#define _SHAPE_H_


#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>

#include "device.h"

struct Vertex;

class Shape
{
public:
	Shape();

	void InitShape(VulkanDevices* devices, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
	void RecordRenderCommands(VkCommandBuffer& command_buffer);
	void RecordTerrainRenderCommands(VkCommandBuffer& command_buffer, int instance_index);
	void CleanUp();
	

protected:
	
	void CreateVertexBuffer(std::vector<Vertex>& vertices);
	void CreateIndexBuffer(std::vector<uint32_t>& indices);

protected:
	VulkanDevices* devices_;

	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;

	VkBuffer index_buffer_;
	VkDeviceMemory index_buffer_memory_;

	uint32_t vertex_count_;
	uint32_t index_count_;

	uint32_t vertex_buffer_offset_;
	uint32_t index_buffer_offset_;
};
#endif