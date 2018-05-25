#include "device.h"
#include "app.h"
#include <iostream>

VulkanDevices::VulkanDevices(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDeviceFeatures required_features, std::vector<const char*> required_extensions)
{
	physical_device_ = VK_NULL_HANDLE;
	logical_device_ = VK_NULL_HANDLE;

	// initialize physical device
	PickPhysicalDevice(instance, surface, required_features, required_extensions);
}

VulkanDevices::~VulkanDevices()
{
	vkDestroyCommandPool(logical_device_, transient_command_pool_, nullptr);
	vkDestroyDevice(logical_device_, nullptr);
}

void VulkanDevices::PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDeviceFeatures required_features, std::vector<const char*> required_extensions)
{
	physical_device_ = VK_NULL_HANDLE;

	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

	if (device_count == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	for (const auto& device : devices)
	{
		if (IsDeviceSuitable(device, surface, required_features, required_extensions))
		{
			physical_device_ = device;
			break;
		}
	}
	
	if (physical_device_ == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	queue_family_indices_ = FindQueueFamilies(physical_device_, surface);
}

void VulkanDevices::CreateLogicalDevice(VkPhysicalDeviceFeatures required_features, std::vector<VkDeviceQueueCreateInfo> required_queues, std::vector<const char*> required_extensions, std::vector<const char*> required_validation_layers)
{
	if (logical_device_ != VK_NULL_HANDLE)
		return;

	// append the required queues with the copy command queue
	float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = queue_family_indices_.transfer_family;
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;
	required_queues.push_back(queue_create_info);

	// setup creation info for logical device
	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<uint32_t>(required_queues.size());
	create_info.pQueueCreateInfos = required_queues.data();
	create_info.pEnabledFeatures = &required_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
	create_info.ppEnabledExtensionNames = required_extensions.data();

	if (ENABLE_VALIDATION_LAYERS)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
		create_info.ppEnabledLayerNames = required_validation_layers.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physical_device_, &create_info, nullptr, &logical_device_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical device!");
	}

	CreateCopyCommandPool();

	vkGetDeviceQueue(logical_device_, queue_family_indices_.graphics_family, 0, &copy_queue_);
}

void VulkanDevices::CreateCopyCommandPool()
{
	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices_.graphics_family;
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	if (vkCreateCommandPool(logical_device_, &pool_info, nullptr, &transient_command_pool_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

VkImageView VulkanDevices::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange.aspectMask = aspect_flags;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	if (vkCreateImageView(logical_device_, &view_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}

	return image_view;
}

bool VulkanDevices::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, VkPhysicalDeviceFeatures required_features, std::vector<const char*> required_extensions)
{
	// get device properties
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);

	// get device features
	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceFeatures(device, &device_features);

	// check the device supports required queues
	QueueFamilyIndices indices = FindQueueFamilies(device, surface);

	// check the device supports required extensions
	bool extensions_supported = CheckDeviceExtensionSupport(device, required_extensions);
	
	// check the device supports the correct swap chain features
	bool swap_chain_adequate = false;
	if (extensions_supported)
	{
		SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(device, surface);
		swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
	}

	return indices.isComplete() && extensions_supported && swap_chain_adequate && device_features.samplerAnisotropy == required_features.samplerAnisotropy && device_features.shaderSampledImageArrayDynamicIndexing == required_features.shaderSampledImageArrayDynamicIndexing;
}

SwapChainSupportDetails VulkanDevices::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

	if (format_count != 0)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

	if (present_mode_count != 0)
	{
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
	}

	return details;
}


bool VulkanDevices::CheckDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char*> required_extensions)
{
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> extensions(required_extensions.begin(), required_extensions.end());

	for (const auto& extension : available_extensions)
	{
		extensions.erase(extension.extensionName);
	}

	return extensions.empty();
}

uint32_t VulkanDevices::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties, VkDeviceSize memory_size)
{
	VkPhysicalDeviceMemoryProperties mem_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties);

	for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i)) && ((mem_properties.memoryTypes[i].propertyFlags & properties) == properties) && mem_properties.memoryHeaps[mem_properties.memoryTypes[i].heapIndex].size >= memory_size)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanDevices::CreateCommandBuffers(VkCommandPool command_pool, VkCommandBuffer* buffers, uint8_t count)
{
	// create the command buffers
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = count;

	if (vkAllocateCommandBuffers(logical_device_, &allocate_info, buffers) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void VulkanDevices::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory)
{
	VkResult result;

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	result = vkCreateBuffer(logical_device_, &buffer_info, nullptr, &buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(logical_device_, buffer, &mem_requirements);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties, mem_requirements.size);

	result = vkAllocateMemory(logical_device_, &alloc_info, nullptr, &buffer_memory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(logical_device_, buffer, buffer_memory, 0);
}

void VulkanDevices::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory)
{
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0;

	if (vkCreateImage(logical_device_, &image_info, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements mem_requirements;
	vkGetImageMemoryRequirements(logical_device_, image, &mem_requirements);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties, mem_requirements.size);

	VkResult result = vkAllocateMemory(logical_device_, &alloc_info, nullptr, &image_memory);
	if ( result != VK_SUCCESS)
	{
		std::cout << result << std::endl;
		std::cin.ignore();
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(logical_device_, image, image_memory, 0);
}

void VulkanDevices::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
	VkCommandBuffer command_buffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	
	if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (HasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}

	VkPipelineStageFlags source_stage;
	VkPipelineStageFlags destination_stage;

	// set up source properties
	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		barrier.srcAccessMask = 0;
		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	else
	{
		throw std::runtime_error("unsupported layout transition!");
	}

	// set up destination properties
	if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)	
	{
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (new_layout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	else
	{
		throw std::runtime_error("unsupported layout transition!");
	}


	vkCmdPipelineBarrier(
		command_buffer,
		source_stage, destination_stage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndSingleTimeCommands(command_buffer);
}

void VulkanDevices::CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, VkDeviceSize offset)
{
	VkCommandBuffer command_buffer = BeginSingleTimeCommands();

	VkBufferCopy copy_region = {};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = offset;
	copy_region.size = size;

	vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
	
	EndSingleTimeCommands(command_buffer);
}

void VulkanDevices::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer command_buffer = BeginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent =
	{
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	EndSingleTimeCommands(command_buffer);
}

void VulkanDevices::CopyDataToBuffer(VkDeviceMemory dst_buffer_memory, void* data, VkDeviceSize size, VkDeviceSize offset)
{
	void* mapped_data; 
	vkMapMemory(logical_device_, dst_buffer_memory, offset, size, 0, &mapped_data);
	memcpy(mapped_data, data, size);
	vkUnmapMemory(logical_device_, dst_buffer_memory);
}

void VulkanDevices::CopyImage(VkImage src, VkImage dst, VkOffset3D dim, VkOffset3D src_offset, VkOffset3D dst_offset)
{
	// start the copy command buffer
	VkCommandBuffer blit_buffer = BeginSingleTimeCommands();
	
	VkImageBlit image_blit = {};
	image_blit.srcOffsets[0] = src_offset;
	image_blit.srcOffsets[1] = src_offset + dim;
	image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.srcSubresource.baseArrayLayer = 0;
	image_blit.srcSubresource.layerCount = 1;
	image_blit.srcSubresource.mipLevel = 0;

	image_blit.dstOffsets[0] = dst_offset;
	image_blit.dstOffsets[1] = dst_offset + dim;
	image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.dstSubresource.baseArrayLayer = 0;
	image_blit.dstSubresource.layerCount = 1;
	image_blit.dstSubresource.mipLevel = 0;

	vkCmdBlitImage(blit_buffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

	// submit the blit command buffer
	EndSingleTimeCommands(blit_buffer);
}

void VulkanDevices::ClearColorImage(VkImage image, VkImageLayout image_layout, VkClearColorValue clear_color)
{
	// clear the image
	VkCommandBuffer clear_buffer = BeginSingleTimeCommands();

	VkImageSubresourceRange image_range = {};
	image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	vkCmdClearColorImage(clear_buffer, image, image_layout, &clear_color, 1, &image_range);

	EndSingleTimeCommands(clear_buffer);
}

VkCommandBuffer VulkanDevices::BeginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = transient_command_pool_;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(logical_device_, &alloc_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;
}

void VulkanDevices::EndSingleTimeCommands(VkCommandBuffer command_buffer, VkSemaphore wait_semaphore)
{
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
	if (wait_semaphore != VK_NULL_HANDLE)
	{
		submit_info.pWaitSemaphores = &wait_semaphore;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitDstStageMask = wait_stages;
	}

	vkQueueSubmit(copy_queue_, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(copy_queue_);

	vkFreeCommandBuffers(logical_device_, transient_command_pool_, 1, &command_buffer);
}

QueueFamilyIndices VulkanDevices::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& queue_family : queue_families)
	{
		// check for graphics queue support
		if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphics_family = i;
			
		}

		// check for transfer queue support
		if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			indices.transfer_family = i;
		}

		// check for compute queue support
		if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			indices.compute_family = i;
		}

		// check for present queue support
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
		if (queue_family.queueCount > 0 && present_support)
		{
			indices.present_family = i;
		}

		if (indices.isComplete())
		{
			break;
		}

		i++;
	}

	if (indices.transfer_family < 0)
	{
		indices.transfer_family = indices.graphics_family;
	}

	return indices;
}

VkFormat VulkanDevices::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}

	}

	throw std::runtime_error("failed to find supported format!");
}


bool VulkanDevices::HasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

std::vector<char> VulkanDevices::ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	size_t file_size = (size_t)file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);

	file.close();

	return buffer;
}

void VulkanDevices::WriteFile(const std::string& filename, const std::string& contents)
{
	std::ofstream file(filename, std::ios::out);

	if (!file.is_open())
	{
		return;
	}

	file.write(contents.c_str(), contents.size());

	file.close();
}

void VulkanDevices::WriteDebugFile(const char* msg)
{
	std::ofstream file;

	file.open("error.txt", std::ofstream::out | std::ofstream::app);

	if (!file.is_open())
	{
		return;
	}

	std::string message(msg);
	message += "\n\n";

	file.write(message.c_str(), message.size());

	file.close();
}

VkOffset3D operator+(VkOffset3D& a, VkOffset3D& b)
{
	return VkOffset3D{ a.x + b.x, a.y + b.y, a.z + b.z };
}