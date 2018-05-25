#include "swap_chain.h"
#include <stdexcept>
#include <algorithm>
#include <array>
#include <iostream>

void VulkanSwapChain::Init(GLFWwindow* window, VkInstance instance)
{
	instance_ = instance;
	window_ = window;
	swap_chain_ = VK_NULL_HANDLE;
	CreateSurface();
}

void VulkanSwapChain::Cleanup()
{
	CleanupSwapChain();

	vkDestroySwapchainKHR(devices_->GetLogicalDevice(), swap_chain_, nullptr);
	vkDestroySurfaceKHR(instance_, surface_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), image_available_semaphore_, nullptr);

	instance_ = VK_NULL_HANDLE;
	window_ = nullptr;
	devices_ = nullptr;
}

void VulkanSwapChain::CleanupSwapChain()
{
	VkDevice device = devices_->GetLogicalDevice();

	for (size_t i = 0; i < swap_chain_images_.size(); i++)
	{
		vkDestroyImageView(device, swap_chain_image_views_[i], nullptr);
	}
	
	vkDestroyImage(device, intermediate_image_, nullptr);
	vkDestroyImageView(device, intermediate_image_view_, nullptr);
	vkFreeMemory(device, intermediate_image_memory_, nullptr);

	vkDestroyImageView(device, depth_image_view_, nullptr);
	vkDestroyImage(device, depth_image_, nullptr);
	vkFreeMemory(device, depth_image_memory_, nullptr);
}

VkResult VulkanSwapChain::PreRender()
{
	// acquire the next image in the swap chain
	VkResult result = vkAcquireNextImageKHR(devices_->GetLogicalDevice(), swap_chain_, std::numeric_limits<uint64_t>::max(), image_available_semaphore_, VK_NULL_HANDLE, &current_image_index_);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return result;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// clear the swap chain image and depth stencil view before rendering
	// transition the buffers to the correct format for clearing
	devices_->TransitionImageLayout(swap_chain_images_[current_image_index_], swap_chain_image_format_, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	devices_->TransitionImageLayout(depth_image_, depth_format_, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// clear the buffers
	VkCommandBuffer clear_buffer = devices_->BeginSingleTimeCommands();

	VkClearColorValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
	VkImageSubresourceRange image_range = {};
	image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	vkCmdClearColorImage(clear_buffer, swap_chain_images_[current_image_index_], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);
	vkCmdClearColorImage(clear_buffer, intermediate_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);

	VkClearDepthStencilValue clear_depth = { 1.0f, 0 };
	image_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	vkCmdClearDepthStencilImage(clear_buffer, depth_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_depth, 1, &image_range);
	
	devices_->EndSingleTimeCommands(clear_buffer);

	// transition buffers back to the correct format
	devices_->TransitionImageLayout(swap_chain_images_[current_image_index_], swap_chain_image_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	devices_->TransitionImageLayout(depth_image_, depth_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return VK_SUCCESS;
}

VkResult VulkanSwapChain::PostRender(VkSemaphore signal_semaphore)
{
	// present the swap chain
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	VkSemaphore signal_semaphores[] = { signal_semaphore };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swap_chains[] = { swap_chain_ };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;
	present_info.pImageIndices = &current_image_index_;
	present_info.pResults = nullptr;

	// present the swap chain image to the screen
	VkResult result = vkQueuePresentKHR(present_queue_, &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		return result;
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}

	vkQueueWaitIdle(present_queue_);

	return result;
}

void VulkanSwapChain::FinalizeIntermediateImage()
{
	// transition the intermediate image to transfersrc layout
	devices_->TransitionImageLayout(swap_chain_images_[current_image_index_], swap_chain_image_format_, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// start the copy command buffer
	VkCommandBuffer blit_buffer = devices_->BeginSingleTimeCommands();

	VkImageBlit image_blit = {};
	image_blit.srcOffsets[0] = { 0, 0, 0 };
	image_blit.srcOffsets[1] = { (int)swap_chain_extent_.width, (int)swap_chain_extent_.height, 1 };
	image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.srcSubresource.baseArrayLayer = 0;
	image_blit.srcSubresource.layerCount = 1;
	image_blit.srcSubresource.mipLevel = 0;

	image_blit.dstOffsets[0] = { 0, 0, 0 };
	image_blit.dstOffsets[1] = { (int)swap_chain_extent_.width, (int)swap_chain_extent_.height, 1 };
	image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.dstSubresource.baseArrayLayer = 0;
	image_blit.dstSubresource.layerCount = 1;
	image_blit.dstSubresource.mipLevel = 0;

	vkCmdBlitImage(blit_buffer, intermediate_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swap_chain_images_[current_image_index_], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

	// submit the blit command buffer
	devices_->EndSingleTimeCommands(blit_buffer, image_available_semaphore_);

	// transition images back to correct layouts
	devices_->TransitionImageLayout(swap_chain_images_[current_image_index_], swap_chain_image_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void VulkanSwapChain::CopyToIntermediateImage(VkImage image, VkImageLayout image_layout)
{
	// transition the images to the correct layouts
	devices_->TransitionImageLayout(image, intermediate_image_format_, image_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// start the copy command buffer
	VkCommandBuffer blit_buffer = devices_->BeginSingleTimeCommands();

	VkImageBlit image_blit = {};
	image_blit.srcOffsets[0] = { 0, 0, 0 };
	image_blit.srcOffsets[1] = { (int)swap_chain_extent_.width, (int)swap_chain_extent_.height, 1 };
	image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.srcSubresource.baseArrayLayer = 0;
	image_blit.srcSubresource.layerCount = 1;
	image_blit.srcSubresource.mipLevel = 0;

	image_blit.dstOffsets[0] = { 0, 0, 0 };
	image_blit.dstOffsets[1] = { (int)swap_chain_extent_.width, (int)swap_chain_extent_.height, 1 };
	image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.dstSubresource.baseArrayLayer = 0;
	image_blit.dstSubresource.layerCount = 1;
	image_blit.dstSubresource.mipLevel = 0;

	vkCmdBlitImage(blit_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, intermediate_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

	devices_->EndSingleTimeCommands(blit_buffer);

	// transition images back to correct layouts
	devices_->TransitionImageLayout(image, intermediate_image_format_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_layout);
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void VulkanSwapChain::CreateSurface()
{
	if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
}

void VulkanSwapChain::CreateSwapChain(VulkanDevices* devices)
{
	devices_ = devices;

	VkDevice vk_device = devices->GetLogicalDevice();
	VkPhysicalDevice vk_physical_device = devices->GetPhysicalDevice();
	intermediate_image_format_ = VK_FORMAT_R32G32B32A32_SFLOAT;
	
	// store the handle of any previous swap chain
	VkSwapchainKHR old_swap_chain = swap_chain_;

	SwapChainSupportDetails swap_chain_support = devices->QuerySwapChainSupport(vk_physical_device, surface_);

	VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);

	VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);

	VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

	// use triple buffering if the swap chain supports it
	uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
	if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
	{
		image_count = swap_chain_support.capabilities.maxImageCount;
	}

	// set up creation info for swap chain
	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface_;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;	// VK_IMAGE_USAGE_TRANSFER_DST_BIT for post processing

																	// set up swap chain usage across multiple queue families
	QueueFamilyIndices indices = VulkanDevices::FindQueueFamilies(vk_physical_device, surface_);
	uint32_t queue_family_indices[] = { (uint32_t)indices.graphics_family, (uint32_t)indices.present_family };

	if (indices.graphics_family != indices.present_family)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	create_info.preTransform = swap_chain_support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;


	// if there is an old swap chain then use it when creating the new swap chain
	if (old_swap_chain != VK_NULL_HANDLE)
		create_info.oldSwapchain = old_swap_chain;
	else
		create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(vk_device, &create_info, nullptr, &swap_chain_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	// if there is an old swap chain
	if (old_swap_chain != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(vk_device);
		vkDestroySwapchainKHR(vk_device, old_swap_chain, nullptr);

		// clean up old swap chain elements
		CleanupSwapChain();
	}

	// get handles for all swap chain images
	vkGetSwapchainImagesKHR(vk_device, swap_chain_, &image_count, nullptr);
	swap_chain_images_.resize(image_count);
	vkGetSwapchainImagesKHR(vk_device, swap_chain_, &image_count, swap_chain_images_.data());

	swap_chain_image_format_ = surface_format.format;
	swap_chain_extent_ = extent;

	// transition the swap chain images to the correct format
	for (int i = 0; i < swap_chain_images_.size(); i++)
	{
		devices_->TransitionImageLayout(swap_chain_images_[i], swap_chain_image_format_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	CreateImageViews();
	CreateIntermediateImage();
	CreateDepthResources();
	CreateSemaphores();

	// get the presentation queue
	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().present_family, 0, &present_queue_);
}

void VulkanSwapChain::CreateImageViews()
{
	swap_chain_image_views_.resize(swap_chain_images_.size());

	// setup creation info for swap chain image views
	for (uint32_t i = 0; i < swap_chain_images_.size(); i++)
	{
		swap_chain_image_views_[i] = devices_->CreateImageView(swap_chain_images_[i], swap_chain_image_format_, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanSwapChain::CreateIntermediateImage()
{
	// create the intermediate storage image
	devices_->CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, intermediate_image_format_, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, intermediate_image_, intermediate_image_memory_);
	intermediate_image_view_ = devices_->CreateImageView(intermediate_image_, intermediate_image_format_, VK_IMAGE_ASPECT_COLOR_BIT);

	// transition to the general image layout
	devices_->TransitionImageLayout(intermediate_image_, intermediate_image_format_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void VulkanSwapChain::CreateDepthResources()
{
	depth_format_ = FindDepthFormat();

	devices_->CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, depth_format_, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_);
	depth_image_view_ = devices_->CreateImageView(depth_image_, depth_format_, VK_IMAGE_ASPECT_DEPTH_BIT);

	devices_->TransitionImageLayout(depth_image_, depth_format_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void VulkanSwapChain::CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &image_available_semaphore_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}

VkSurfaceFormatKHR VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
	if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& available_format : available_formats)
	{
		if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return available_format;
		}
	}

	return available_formats[0];
}

VkPresentModeKHR VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> available_present_modes)
{
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	for (const auto& available_present_mode : available_present_modes)
	{
		if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return available_present_mode;
		else if (available_present_mode == VK_PRESENT_MODE_FIFO_KHR)
			best_mode = available_present_mode;
	}

	return best_mode;
}

VkExtent2D VulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetWindowSize(window_, &width, &height);

		VkExtent2D actual_extent = { width, height };

		actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));

		return actual_extent;
	}
}

VkFormat VulkanSwapChain::FindDepthFormat()
{
	return devices_->FindSupportedFormat(
	{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
}