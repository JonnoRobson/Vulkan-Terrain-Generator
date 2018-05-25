#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "device.h"

#define MAX_TEXTURE_RESOLUTION 2048

class Texture
{
public:
	enum class MapType
	{
		AMBIENT,
		DIFFUSE,
		SPECULAR,
		SPECULAR_HIGHLIGHT,
		EMISSIVE,
		NORMAL,
		ALPHA,
		REFLECTION
	};

public:
	Texture();
	~Texture();

	void Init(VulkanDevices* devices, std::string filename);
	void Cleanup();

	VkImageView GetImageView() { return texture_image_view_; }
	VkSampler GetSampler() { return texture_sampler_; }

	void AddMapType(MapType map_type);
	std::vector<MapType> GetMapTypes() { return map_types_; }

	void IncrementUsageCount() { usage_count_++; }
	void DecrementUsageCount() { usage_count_--; }
	uint16_t GetUsageCount() { return usage_count_; }
	
	std::string GetTextureName() { return texture_name_; }

	VkDeviceSize GetImageSize() { return image_size_; }

protected:
	void InitSampler(VulkanDevices* devices);

protected:
	VkImage texture_image_;
	VkDeviceMemory texture_image_memory_;
	VkImageView texture_image_view_;
	VkSampler texture_sampler_;

	VkDeviceSize image_size_;
	std::string texture_name_;
	std::vector<MapType> map_types_;
	uint16_t usage_count_;

	VkDevice vk_device_handle_;
};

#endif