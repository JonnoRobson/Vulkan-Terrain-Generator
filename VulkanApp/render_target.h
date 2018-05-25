#ifndef _RENDER_TARGET_H_
#define _RENDER_TARGET_H_

#include <vulkan/vulkan.h>
#include <vector>

#include "device.h"

class VulkanRenderTarget
{
public:
	void Init(VulkanDevices* devices, VkFormat format, uint32_t width, uint32_t height, uint32_t count, bool depth_enabled);
	void Cleanup();
	
	void ClearImage(VkClearColorValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f }, int image_index = -1);
	void ClearDepth();

	inline std::vector<VkImage>& GetImages() { return render_target_images_; }
	inline std::vector <VkImageView>& GetImageViews() { return render_target_image_views_; }
	inline std::vector<VkDeviceMemory>& GetImageMemories() { return render_target_image_memories_; }

	inline VkImage GetDepthImage() { return render_target_depth_image_; }
	inline VkImageView GetDepthImageView() { return render_target_depth_image_view_; }
	inline VkDeviceMemory GetDepthImageMemory() { return render_target_depth_image_memory_; }

	inline VkFormat GetRenderTargetFormat() { return render_target_format_; }
	inline VkFormat GetRenderTargetDepthFormat() { return render_target_depth_format_; }

	inline int GetRenderTargetCount() { return render_target_images_.size(); }

protected:
	VulkanDevices* devices_;

	VkFormat render_target_format_;
	std::vector<VkImage> render_target_images_;
	std::vector<VkDeviceMemory> render_target_image_memories_;
	std::vector<VkImageView> render_target_image_views_;


	VkFormat render_target_depth_format_;
	VkImage render_target_depth_image_;
	VkDeviceMemory render_target_depth_image_memory_;
	VkImageView render_target_depth_image_view_;

};

#endif