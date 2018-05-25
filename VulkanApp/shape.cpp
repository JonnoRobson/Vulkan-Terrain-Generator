#include "shape.h"
#include "mesh.h"
#include "renderer.h"
#include "terrain_generator.h"

Shape::Shape()
{
	devices_ = nullptr;

	vertex_buffer_ = VK_NULL_HANDLE;
	vertex_buffer_memory_ = VK_NULL_HANDLE;

	index_buffer_ = VK_NULL_HANDLE;
	index_buffer_memory_ = VK_NULL_HANDLE;

}

void Shape::InitShape(VulkanDevices* devices, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	devices_ = devices;
	
	CreateVertexBuffer(vertices);
	CreateIndexBuffer(indices);
}

void Shape::CleanUp()
{
	// free the vertex and index buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), vertex_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), index_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), index_buffer_memory_, nullptr);
}

void Shape::CreateVertexBuffer(std::vector<Vertex>& vertices)
{
	vertex_count_ = static_cast<uint32_t>(vertices.size());
	VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

	// create the vertex buffer
	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_buffer_memory_);

	// create a staging buffer to copy the data from
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buffer, staging_buffer_memory);
		
	// copy the data to the staging buffer
	void* data;
	vkMapMemory(devices_->GetLogicalDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, vertices.data(), (size_t)buffer_size);
	vkUnmapMemory(devices_->GetLogicalDevice(), staging_buffer_memory);

	// copy the data from the staging buffer to the vertex buffer
	devices_->CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

	// clean up the staging buffer now that it is no longer needed
	vkDestroyBuffer(devices_->GetLogicalDevice(), staging_buffer, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), staging_buffer_memory, nullptr);
}

void Shape::CreateIndexBuffer(std::vector<uint32_t>& indices)
{
	index_count_ = static_cast<uint32_t>(indices.size());
	VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

	// create the index buffer
	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);

	// create a staging buffer to copy the data from
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buffer, staging_buffer_memory);

	// copy the data to the staging buffer
	void* data;
	vkMapMemory(devices_->GetLogicalDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, indices.data(), (size_t)buffer_size);
	vkUnmapMemory(devices_->GetLogicalDevice(), staging_buffer_memory);

	// copy the data from the staging buffer to the vertex buffer
	devices_->CopyBuffer(staging_buffer, index_buffer_, buffer_size);

	// clean up the staging buffer now that it is no longer needed
	vkDestroyBuffer(devices_->GetLogicalDevice(), staging_buffer, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), staging_buffer_memory, nullptr);
}

void Shape::RecordRenderCommands(VkCommandBuffer& command_buffer)
{
	// bind the vertex and index buffers
	VkBuffer vertex_buffers[] = { vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
		
	// execute a draw command
	vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
}

void Shape::RecordTerrainRenderCommands(VkCommandBuffer& command_buffer, int instance_index)
{
	// bind the vertex and index buffers
	VkBuffer vertex_buffers[] = { vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);

	// execute a draw command
	vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, instance_index);
}