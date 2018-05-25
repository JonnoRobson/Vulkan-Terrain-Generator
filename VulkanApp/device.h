#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <vulkan/vulkan.h>
#include <vector>

typedef bool(*check_function)(void);

struct QueueFamilyIndices
{
	int graphics_family = -1;
	int transfer_family = -1;
	int present_family = -1;
	int compute_family = -1;

	bool isComplete()
	{
		return graphics_family >= 0 && transfer_family >= 0 && present_family >= 0 && compute_family >= 0;
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

class VulkanDevices
{
public:
	VulkanDevices(VkInstance, VkSurfaceKHR, VkPhysicalDeviceFeatures, std::vector<const char*>);
	~VulkanDevices();

	void CreateLogicalDevice(VkPhysicalDeviceFeatures, std::vector<VkDeviceQueueCreateInfo>, std::vector<const char*>, std::vector<const char*>);

	void CreateBuffer(VkDeviceSize, VkBufferUsageFlags, VkMemoryPropertyFlags, VkBuffer&, VkDeviceMemory&);
	void CreateImage(uint32_t, uint32_t, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
	VkImageView CreateImageView(VkImage, VkFormat, VkImageAspectFlags);
	void CreateCommandBuffers(VkCommandPool command_pool, VkCommandBuffer* buffers, uint8_t count = 1);

	void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, VkDeviceSize offset = 0);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void CopyDataToBuffer(VkDeviceMemory dst_buffer_memory, void* data, VkDeviceSize size, VkDeviceSize offset = 0);
	void CopyImage(VkImage src_image, VkImage dst_image, VkOffset3D dimensions, VkOffset3D src_offset = { 0, 0, 0 }, VkOffset3D dst_offset = { 0, 0, 0 });
	void ClearColorImage(VkImage image, VkImageLayout image_layout, VkClearColorValue color);

	void TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout);

	VkPhysicalDevice GetPhysicalDevice() { return physical_device_; }
	VkDevice GetLogicalDevice() { return logical_device_; }
	QueueFamilyIndices GetQueueFamilyIndices() { return queue_family_indices_; }

	uint32_t FindMemoryType(uint32_t, VkMemoryPropertyFlags, VkDeviceSize);
	VkFormat FindSupportedFormat(const std::vector<VkFormat>&, VkImageTiling, VkFormatFeatureFlags);
	
	static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice, VkSurfaceKHR);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice, VkSurfaceKHR);

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer, VkSemaphore = VK_NULL_HANDLE);

protected:
	void CreateCopyCommandPool();

	bool HasStencilComponent(VkFormat format);

	void PickPhysicalDevice(VkInstance, VkSurfaceKHR, VkPhysicalDeviceFeatures, std::vector<const char*>);
	bool IsDeviceSuitable(VkPhysicalDevice, VkSurfaceKHR, VkPhysicalDeviceFeatures, std::vector<const char*>);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice, std::vector<const char*>);


protected:
	VkPhysicalDevice physical_device_;
	VkDevice logical_device_;
	QueueFamilyIndices queue_family_indices_;

	VkCommandPool transient_command_pool_;
	VkQueue copy_queue_;

public:
	static std::vector<char> ReadFile(const std::string& filename);
	static void WriteFile(const std::string& filename, const std::string& contents);
	static void WriteDebugFile(const char* msg);
};

VkOffset3D operator+(VkOffset3D& a, VkOffset3D& b);
#endif