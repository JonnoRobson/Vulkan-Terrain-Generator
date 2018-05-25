#ifndef _APP_H_
#define _APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <algorithm>
#include <assert.h>

#include "swap_chain.h"
#include "renderer.h"
#include "device.h"
#include "texture.h"
#include "camera.h"
#include "input.h"

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);

#ifdef NDEBUG
#define ENABLE_VALIDATION_LAYERS false
#else
#define ENABLE_VALIDATION_LAYERS true
#endif


class App
{
public:
	virtual void Run();

	void RecreateSwapChain();


public:
	Input* input_;

protected:
	virtual bool InitVulkan();
	virtual bool InitResources();
	virtual bool InitWindow();
	virtual void MainLoop();
	virtual void CleanUp();

	virtual void Update();
	virtual void DrawFrame();

	virtual bool CreateInstance();
	bool ValidateExtensions();
	virtual std::vector<const char*> GetRequiredExtensions();
	virtual bool CheckValidationLayerSupport();
	void SetupDebugCallback();
	void InitDevices();

protected:
	GLFWwindow *window_;
	VulkanDevices *devices_;
	VulkanSwapChain* swap_chain_;
	VulkanRenderer* renderer_;

	VkInstance vk_instance_;
	VkDebugReportCallbackEXT vk_callback_;

	Camera camera_;

	float current_time_;
	float prev_time_;
	float frame_time_;

	const int window_width_ = 1920;
	const int window_height_ = 1080;

	bool validation_layers_enabled_;

	const std::vector<const char*> validation_layers_ = {
		"VK_LAYER_LUNARG_standard_validation",
		"VK_LAYER_LUNARG_monitor"
	};

	const std::vector<const char*> device_extensions_ = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	std::string debug_msg_;

protected:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData);

	static void keyCallback(
		GLFWwindow* window,
		int key,
		int scancode,
		int action,
		int mods
	);

	static void cursorPositionCallback(
		GLFWwindow* window,
		double xpos,
		double ypos
	);

	static void mouseButtonCallback(
		GLFWwindow* window,
		int button,
		int action,
		int mods
	);

	static void OnWindowResized(GLFWwindow* window, int width, int height);
};


#endif