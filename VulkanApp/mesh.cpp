#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <iostream>
#include <thread>
#include "renderer.h"

Mesh::Mesh()
{
	world_matrix_ = glm::mat4(1.0f);
	vk_device_handle_ = VK_NULL_HANDLE;
	min_vertex_ = glm::vec3(1e9f, 1e9f, 1e9f);
	max_vertex_ = glm::vec3(-1e9f, -1e9f, -1e9f);
}

Mesh::~Mesh()
{
	// clean up  shapes
	for (Shape* shape : mesh_shapes_)
	{
		shape->CleanUp();
		delete shape;
	}
	mesh_shapes_.clear();
	
	vk_device_handle_ = VK_NULL_HANDLE;
}

void Mesh::UpdateWorldMatrix(glm::mat4 world_matrix)
{
	world_matrix_ = world_matrix;
}

void Mesh::CreateModelMesh(VulkanDevices* devices, std::string filename)
{
	vk_device_handle_ = devices->GetLogicalDevice();

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	std::cout << "Loading model file: " << filename << std::endl;

	std::string mat_dir = "../res/materials/";

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str(), mat_dir.c_str()))
	{
		throw std::runtime_error(err);
	}

	std::cout << "Model contains " << shapes.size() << " shapes" << std::endl;

	std::cout << "Model contains " << materials.size() << " unique materials" << std::endl;

	
	size_t index_count = 0;
	for (const tinyobj::shape_t& shape : shapes)
	{
		index_count += shape.mesh.indices.size();
	}

	std::cout << "Model contains " << index_count << " indices" << std::endl;

	// create the mesh materials
	for (tinyobj::material_t material : materials)
	{

	}

	// multithread shape loading
	const int thread_count = 10;
	std::thread shape_threads[thread_count];

	// determine how many shapes are calculated per thread
	const int shapes_per_thread = shapes.size() / thread_count;
	int leftover_shapes = shapes.size() % thread_count;

	// create mutex for sending shape data to the renderer
	std::mutex shape_mutex;

	int shape_index = 0;

	// send the shapes to the threads
	for (int thread_index = 0; thread_index < thread_count; thread_index++)
	{
		std::vector<tinyobj::shape_t*> thread_shapes;

		
		// determine the shapes that will be sent to this thread
		for (int i = 0; i < shapes_per_thread; i++)
		{
			thread_shapes.push_back(&shapes[shape_index]);
			shape_index++;
		}

		// if there are still leftover shapes add one to this vector
		if (leftover_shapes > 0)
		{
			thread_shapes.push_back(&shapes[shape_index]);
			shape_index++;
			leftover_shapes--;
		}

		// start the thread for this set of shapes
		shape_threads[thread_index] = std::thread(&Mesh::LoadShapeThreaded, this, &shape_mutex, devices, &attrib, &materials, thread_shapes);
	}

	for (int i = 0; i < thread_count; i++)
	{
		shape_threads[i].join();
	}

}

void Mesh::CreateTerrainMesh(VulkanDevices* devices, int terrain_size)
{
	// store terrain size plus one
	int terrain_size_plus_one = terrain_size + 1;

	// calculate vertex and index counts
	int vertex_count = (terrain_size) * (terrain_size) * 4;
	int index_count = (terrain_size) * (terrain_size) * 6;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(vertex_count);
	indices.resize(index_count);

	Vertex v0, v1, v2, v3;
	for (int y = 0; y < terrain_size; y++)
	{
		for (int x = 0; x < terrain_size; x++)
		{
			// bottom left
			v0.pos = glm::vec3((float)x / (float)terrain_size, (float)y / (float)terrain_size, 0.0f) * 2.0f - 1.0f;
			v0.tex_coord = glm::vec2(((float)x / (float)terrain_size), ((float)y / (float)terrain_size));
			v0.normal = glm::vec3(0, 0, 0);
			v0.mat_index = 0;
			int v0_index = vertices.size();
			vertices.push_back(v0);

			// top left
			v1.pos = glm::vec3((float)x / (float)terrain_size, (float)(y + 1) / (float)terrain_size, 0.0f) * 2.0f - 1.0f;
			v1.tex_coord = glm::vec2(((float)x / (float)terrain_size), ((float)(y + 1) / (float)terrain_size));
			v1.normal = glm::vec3(0, 0, 0);
			v1.mat_index = 0;
			int v1_index = vertices.size();
			vertices.push_back(v1);

			// top right
			v2.pos = glm::vec3((float)(x + 1) / (float)terrain_size, (float)(y + 1) / (float)terrain_size, 0.0f) * 2.0f - 1.0f;
			v2.tex_coord = glm::vec2(((float)(x + 1) / (float)terrain_size), ((float)(y + 1) / (float)terrain_size));
			v2.normal = glm::vec3(0, 0, 0);
			v2.mat_index = 0;
			int v2_index = vertices.size();
			vertices.push_back(v2);

			// bottom right
			v3.pos = glm::vec3((float)(x + 1) / (float)terrain_size, (float)y / (float)terrain_size, 0.0f) * 2.0f - 1.0f;
			v3.tex_coord = glm::vec2(((float)(x + 1) / (float)terrain_size), ((float)y / (float)terrain_size));
			v3.normal = glm::vec3(0, 0, 0);
			v3.mat_index = 0;
			int v3_index = vertices.size();
			vertices.push_back(v3);
			
			// setup indices
			indices.push_back(v0_index);
			indices.push_back(v1_index);
			indices.push_back(v2_index);
			indices.push_back(v0_index);
			indices.push_back(v2_index);
			indices.push_back(v3_index);
		}
	}
	
	Shape* terrain_shape = new Shape();
	terrain_shape->InitShape(devices, vertices, indices);
	mesh_shapes_.push_back(terrain_shape);
}

void Mesh::CreateTessellatedTerrainMesh(VulkanDevices* devices, int terrain_size)
{
	// store terrain size plus one as its need A LOT
	int terrain_size_plus_one = terrain_size + 1;

	// calculate vertex and index counts
	int vertex_count = (terrain_size_plus_one) * (terrain_size_plus_one);
	int index_count = (terrain_size * terrain_size) * 12;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(vertex_count);
	indices.resize(index_count);

	float f_size = (float)terrain_size;

	for (int x = 0; x < terrain_size_plus_one; x++)
	{
		for (int y = 0; y < terrain_size_plus_one; y++)
		{
			float fX = ((float)x / f_size) - 0.5f;
			float fY = ((float)y / f_size) - 0.5f;
			int vertex = x + y * (terrain_size_plus_one);
			vertices[vertex].pos = glm::vec3(fX, fY, 0);
			vertices[vertex].tex_coord = glm::vec2(fX + 0.5f, 1.0f - (fY + 0.5f));
			vertices[vertex].normal = glm::vec3(0.0f, 0.0f, 0.0f);
			vertices[vertex].mat_index = 0;
		}
	}

	int index = 0;

	for (int x = 0; x < terrain_size; x++)
	{
		for (int y = 0; y < terrain_size; y++)
		{
			// define 12 control points per quad
			// 0-3 are vertices of the quad itself
			indices[index] = (y + 0) + (x + 0) * (terrain_size_plus_one);
			index++;
			indices[index] = (y + 1) + (x + 0) * (terrain_size_plus_one);
			index++;
			indices[index] = (y + 0) + (x + 1) * (terrain_size_plus_one);
			index++;
			indices[index] = (y + 1) + (x + 1) * (terrain_size_plus_one);

			// 4-5 are +x neighbour
			indices[index] = clamp(y + 0, 0, terrain_size) + clamp(x + 2, 0, terrain_size) * terrain_size_plus_one;
			index++;
			indices[index] = clamp(y + 1, 0, terrain_size) + clamp(x + 2, 0, terrain_size) * terrain_size_plus_one;
			index++;

			// 6-7 are +y neighbour
			indices[index] = clamp(y + 2, 0, terrain_size) + clamp(x + 0, 0, terrain_size) * terrain_size_plus_one;
			index++;
			indices[index] = clamp(y + 2, 0, terrain_size) + clamp(x + 1, 0, terrain_size) * terrain_size_plus_one;
			index++;

			// 8-9 are -x neighbour
			indices[index] = clamp(y + 0, 0, terrain_size) + clamp(x - 1, 0, terrain_size) * terrain_size_plus_one;
			index++;
			indices[index] = clamp(y + 1, 0, terrain_size) + clamp(x - 1, 0, terrain_size) * terrain_size_plus_one;
			index++;

			// 10-11 are -y neighbour
			indices[index] = clamp(y - 1, 0, terrain_size) + clamp(x - 1, 0, terrain_size) * terrain_size_plus_one;
			index++;
			indices[index] = clamp(y - 1, 0, terrain_size) + clamp(x - 1, 0, terrain_size) * terrain_size_plus_one;
			index++;
		}
	}

	Shape* terrain_shape = new Shape();
	terrain_shape->InitShape(devices, vertices, indices);
	mesh_shapes_.push_back(terrain_shape);
}

void Mesh::LoadShapeThreaded(std::mutex* shape_mutex, VulkanDevices* devices, tinyobj::attrib_t* attrib, std::vector<tinyobj::material_t>* materials, std::vector<tinyobj::shape_t*> shapes)
{
	for (const auto& shape : shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices = {};
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		Shape* mesh_shape = new Shape();

		int face_index = 0;

		for (const auto& index : shape->mesh.indices)
		{
			Vertex vertex = {};

			if (index.vertex_index >= 0)
			{
				vertex.pos = {
					attrib->vertices[3 * index.vertex_index + 0],
					attrib->vertices[3 * index.vertex_index + 2],
					attrib->vertices[3 * index.vertex_index + 1]
				};
			}
			else
			{
				vertex.pos = { 0.0f, 0.0f, 0.0f };
			}

			if (index.texcoord_index >= 0)
			{
				vertex.tex_coord = {
					attrib->texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib->texcoords[2 * index.texcoord_index + 1]
				};
			}
			else
			{
				vertex.tex_coord = { 0.0f, 0.0f };
			}

			if (index.normal_index >= 0)
			{
				vertex.normal = {
					-attrib->normals[3 * index.normal_index + 0],
					attrib->normals[3 * index.normal_index + 2],
					attrib->normals[3 * index.normal_index + 1]
				};
			}
			else
			{
				vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);
			}

			// material index
			vertex.mat_index = 0;
			
			if (unique_vertices.count(vertex) == 0)
			{
				unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);

				// test to see if this vertex is outside the current bounds
				if (vertex.pos.x < min_vertex_.x)
					min_vertex_.x = vertex.pos.x;
				else if (vertex.pos.x > max_vertex_.x)
					max_vertex_.x = vertex.pos.x;

				if (vertex.pos.y < min_vertex_.y)
					min_vertex_.y = vertex.pos.y;
				else if (vertex.pos.y > max_vertex_.y)
					max_vertex_.y = vertex.pos.y;

				if (vertex.pos.z < min_vertex_.z)
					min_vertex_.z = vertex.pos.z;
				else if (vertex.pos.z > max_vertex_.z)
					max_vertex_.z = vertex.pos.z;
			}

			indices.push_back(unique_vertices[vertex]);

			if (indices.size() % 3 == 0)
				face_index++;
		}

		std::unique_lock<std::mutex> shape_lock(*shape_mutex);
		mesh_shape->InitShape(devices, vertices, indices);
		mesh_shapes_.push_back(mesh_shape);
		shape_lock.unlock();
	}
}

void Mesh::RecordRenderCommands(VkCommandBuffer& command_buffer)
{
	for (Shape* shape : mesh_shapes_)
	{
		shape->RecordRenderCommands(command_buffer);
	}
}

void Mesh::RecordTerrainRenderCommands(VkCommandBuffer& command_buffer, int instance_index)
{
	for (Shape* shape : mesh_shapes_)
	{
		shape->RecordTerrainRenderCommands(command_buffer, instance_index);
	}
}