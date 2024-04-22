#include "RTSystem.h"
#include "vulkan/vulkan.h"
#include "FileHelp.h"
#include <iostream>
#include <optional>
#include "platform.h"
#ifdef PLATFORM_WIN
#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan_win32.h"
#endif // PLATFORM_WIN
#ifdef PLATFORM_LIN
#include "vulkan/vulkan_xcb.h"
#define VK_USE_PLATFORM_XCB_KHR
#endif //PLATFORM_LIN
#include <set>
#include <algorithm>
#include "MathHelpers.h"
#include <chrono>
#include "SceneGraph.h"
#include "shaderc/shaderc.hpp"
#include <thread>
#include "stb_image.h"
#include "SystemCommon.h"
#include "SystemCommonTypes.h"
#include <glm/mat4x4.hpp> //TODO: TEMP
//https://vulkan-tutorial.com
//https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingsetup




void RTSystem::initVulkan(DrawList drawList, std::string cameraName) {
	//Vertex shader
	vertices = drawList.vertexPool;
	meshMinMax = drawList.meshMinMaxVerts;
	transformPoolsMesh = drawList.meshTransformPools;
	indexPoolsMesh = drawList.meshIndexPools;


	//Materials
	transformNormalPoolsMesh = drawList.meshNormalTransformPools;
	transformEnvironmentPoolsMesh = drawList.meshEnvironmentTransformPools;
	meshMaterials = drawList.meshMaterials;
	rawTextures = drawList.textureMaps;
	rawCubes = drawList.cubeMaps;

	//Lights
	rawEnvironment = drawList.environmentMap;
	lightPool = drawList.lights;
	worldTolightPool = drawList.worldToLights;
	worldTolightPerspPool = drawList.worldToLightsPersp;

	//Animate and bounding box
	drawPools = drawList.drawPools;
	boundingSpheresMesh = drawList.meshBoundingSpheres;
	nodeDrivers = drawList.nodeDrivers;
	cameraDrivers = drawList.cameraDrivers;

	//Cameras
	cameras = drawList.cameras;

	int findCamera = 0;
	for (; findCamera < cameras.size(); findCamera++) {
		if (cameras[findCamera].name.compare(cameraName) == 0) break;
	}
	if (findCamera < cameras.size()) {
		currentCamera = findCamera;
	}

	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createStorageImages();
	createImageViews();
	createCommands();
	createVertexBuffer();
	createIndexBuffers();
	createTransformBuffers();
	createAccelereationStructures();
	createDescriptorSetLayout();
	createRenderPasses();
	createGraphicsPipelines();
	createShaderBindingTable();
	createFramebuffers();
	createTextureImages();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	

	if (renderToWindow) {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, imageAvailableSemaphores.data() + i) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, renderFinishedSemaphores.data() + i) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, inFlightFences.data() + i) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Unable to create a semaphore or fence in RTSystem.");
			}
		}
		for (int i = 0; i < imageCount; i++) {
			transitionImageLayout(rtImages[i], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
		}
	}

}

void RTSystem::listPhysicalDevices() {
	createInstance(false);
	std::cout << "Vulkan found the following supported physical devices: " << std::endl;

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("ERROR: No GPUs found with Vulkan support in RTSystem.");
	}
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
	for (const VkPhysicalDevice device : physicalDevices) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		std::cout << deviceProperties.deviceName << std::endl;
	}
}


void RTSystem::setDriverRuntime(float time) {
	for (size_t ind = 0; ind < cameraDrivers.size(); ind++) {
		cameraDrivers[ind].currentRuntime = time;
	}
	for (size_t ind = 0; ind < nodeDrivers.size(); ind++) {
		nodeDrivers[ind].currentRuntime = time;
	}
}

void RTSystem::cleanup() {
#ifndef NDEBUG
	std::cout << "Cleaning up Vulkan Mode." << std::endl;
#endif // DEBUG

	//TODO: Cleanup acc structures here!

	cleanupSwapChain();
	for (VkImageView texImageView : textureImageViews) {
		vkDestroyImageView(device, texImageView, nullptr);
	}
	for (VkImage texImage : textureImages) {
		vkDestroyImage(device, texImage, nullptr);
	}
	for (VkDeviceMemory texMemory : textureImageMemorys) {
		vkFreeMemory(device, texMemory, nullptr);
	}
	for (VkImageView texImageView : cubeImageViews) {
		vkDestroyImageView(device, texImageView, nullptr);
	}
	for (VkImage texImage : cubeImages) {
		vkDestroyImage(device, texImage, nullptr);
	}
	for (VkDeviceMemory texMemory : cubeImageMemorys) {
		vkFreeMemory(device, texMemory, nullptr);
	}
	if (rawEnvironment.has_value()) {
		vkDestroyImageView(device, environmentImageView, nullptr);
		vkDestroyImage(device, environmentImage, nullptr);
		vkFreeMemory(device, environmentImageMemory, nullptr);
	}
	vkDestroyImageView(device, LUTImageView, nullptr);
	vkDestroyImage(device, LUTImage, nullptr);
	vkFreeMemory(device, LUTImageMemory, nullptr);
	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		vkDestroyBuffer(device, uniformBuffersCamera[frame], nullptr);
		vkFreeMemory(device, uniformBuffersMemoryCamera[frame], nullptr);
	}
	vkDestroyDescriptorPool(device, descriptorPoolHDR, nullptr);
	for (int i = 0; i < 2; i++) {
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts[i], nullptr);
	}
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayoutRT, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayoutFinal, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	//vkDestroyInstance(instance, nullptr); //Instance detroying crashes program
	//At the moment Id prefer to continue owning the application than
	//Handle a tiny amount of memory in the sole case of a closed window.
}


void RTSystem::createInstance(bool verbose) {
	if (enableValidationLayers && !checkValidationSupport()) {
		throw std::runtime_error("ERROR: One of the requested validation layers is not available in RTSystem.");
	}
	//Application info
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Back End";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Vulkan Back End";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	//Create info
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	//Query available extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "Querying available extensions: " << std::endl;
	for (const auto& extension : extensions) {
		std::cout << extension.extensionName << std::endl;
	}



	//Extensions needed to support windows system
#ifdef PLATFORM_WIN


	std::array<const char*, 5> requiredExtensionArray = std::array<const char*, 5>();
	requiredExtensionArray[0] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
	requiredExtensionArray[1] = VK_KHR_SURFACE_EXTENSION_NAME;
	requiredExtensionArray[2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	requiredExtensionArray[3] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	requiredExtensionArray[4] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
	instanceCreateInfo.enabledExtensionCount = requiredExtensionArray.size();
	instanceCreateInfo.ppEnabledExtensionNames = requiredExtensionArray.data();
#endif // PLATFORM_WIN

#ifdef PLATFORM_LIN

#endif // PLATFORM_LIN


	instanceCreateInfo.enabledLayerCount = 0;
	if (enableValidationLayers) {
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failed to create Vulkan instance in RTSystem.");
	}



	//Query supported extensions
	extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
	if (verbose) {
		std::cout << "Available extensions:\n";
		for (const VkExtensionProperties& extension : availableExtensions) {
			std::cout << '\t' << extension.extensionName << std::endl;
		}
	}
}

void RTSystem::setupDebugMessenger() {
	if (!enableValidationLayers) return;
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to set up debug messenger in RTSystem.");
	}
}

void RTSystem::createSurface() {
#ifdef PLATFORM_WIN


	VkWin32SurfaceCreateInfoKHR surfaceCreateInfoWin{};
	surfaceCreateInfoWin.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfoWin.hwnd = mainWindow->getHwnd();
	surfaceCreateInfoWin.hinstance = GetModuleHandle(nullptr);
	if (vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfoWin, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failed to create surface for main window in RTSystem.");
	}
#endif // PLATFORM_WIN
#ifdef PLATFORM_LIN


	VkXcbSurfaceCreateInfoKHR surfaceCreateInfoLin{};
	surfaceCreateInfoLin.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfoLin.connection = mainWindow->getConnection();
	surfaceCreateInfoLin.window = mainWindow->getWindow();
	vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfoLin, nullptr, &surface);
#endif // PLATFORM_LIN
}


bool RTSystem::checkValidationSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;
		for (const VkLayerProperties& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
			}
		}

		if (!layerFound) return false;
	}
	return true;
}


QueueFamilyIndices RTSystem::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int currentIndex = 0;
	for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, currentIndex, surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = currentIndex;
		}
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = currentIndex;
		}
		if (indices.isComplete()) break;
		currentIndex++;
	}
	return indices;
}

bool RTSystem::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
		nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
		availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const VkExtensionProperties& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}


SwapChainSupportDetails RTSystem::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails queriedDetails;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
		&queriedDetails.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0) {
		queriedDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, queriedDetails.formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		queriedDetails.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, queriedDetails.presentModes.data());
	}
	return queriedDetails;
}

int RTSystem::isDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	if (!deviceFeatures.samplerAnisotropy) return -1;

	QueueFamilyIndices indices = findQueueFamilies(device);
	if (!indices.isComplete() || !CheckDeviceExtensionSupport(device)) {
		return -1;
	}
	bool swapChainSupported = false;
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
	if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
		std::cout << "ERROR: Failure in finding supported swapChain attributes in RTSystem." << std::endl;
		return -1;
	}
	int score = 0;
	//Maximally prefer a discrete GPU
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 100000;
	}

	//Prefer larger texter resolution and framebuffer size
	score += deviceProperties.limits.maxImageDimension2D;
	score += deviceProperties.limits.maxFramebufferHeight;
	score += deviceProperties.limits.maxFramebufferWidth;

	return score;
}


void RTSystem::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("ERROR: No GPUs found with Vulkan support in RTSystem.");
	}
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
	bool deviceFound = false;
	for (const VkPhysicalDevice device : physicalDevices) {
		int currentScore = isDeviceSuitable(device);
		if (currentScore > 0) {

			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);
			if (std::string(deviceProperties.deviceName).compare(deviceName) == 0) {
				deviceFound = true;
				physicalDevice = device;
			}
		}
	}
	if (!deviceFound) {
		throw std::runtime_error("ERROR: Unable to find user-requested device. Use the argument --list-physical-devices to see the names of all available devices.");
	}

	//Fill  in RT properties
	VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	prop2.pNext = &rtProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);
}

void RTSystem::createLogicalDevice() {
	familyIndices = findQueueFamilies(physicalDevice);
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { familyIndices.graphicsFamily.value(), familyIndices.presentFamily.value() };
	float queuePriority = 1.0;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		deviceQueueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount =
		static_cast<uint32_t>(deviceQueueCreateInfos.size());
	createInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	createInfo.enabledExtensionCount =
		static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.enabledLayerCount = 0;
	VkPhysicalDeviceFeatures2 phyDeviceFeatures = {};
	VkPhysicalDeviceVulkan12Features deviceFeatures = {};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures{};
	pipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	accFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	phyDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	accFeatures.pNext = &pipelineFeatures;
	deviceFeatures.pNext = &accFeatures;
	phyDeviceFeatures.pNext = &deviceFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &phyDeviceFeatures);
	deviceFeatures.bufferDeviceAddress = VK_TRUE;
	phyDeviceFeatures.features.samplerAnisotropy = VK_TRUE;
	accFeatures.accelerationStructure = VK_TRUE;
	pipelineFeatures.rayTracingPipeline = VK_TRUE;
	createInfo.pNext = &phyDeviceFeatures;
	createInfo.pEnabledFeatures = nullptr;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount =
			static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device)
		!= VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failure when trying to create logical device in RTSystem.");
	}
	vkGetDeviceQueue(device, familyIndices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, familyIndices.presentFamily.value(), 0, &presentQueue);
}

void RTSystem::createSwapChain() {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkExtent2D extent;
	extent.width = mainWindow->resolution.first;
	extent.height = mainWindow->resolution.second;
	if (renderToWindow) {
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		extent = chooseSwapExtent(swapChainSupport.capabilities, mainWindow->resolution);
		imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}


		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t queueFamiliyIndices[] = { familyIndices.graphicsFamily.value(), familyIndices.presentFamily.value() };
		if (familyIndices.graphicsFamily != familyIndices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamiliyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create swap chain in RTSystem.");
		}

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	}
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

VkImageView RTSystem::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageViewType viewType, int levels) {
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = viewType;
	createInfo.format = format;
	VkComponentSwizzle swizzle = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.r = swizzle;
	createInfo.components.g = swizzle;
	createInfo.components.b = swizzle;
	createInfo.components.a = swizzle;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = levels;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &createInfo, nullptr, &imageView)
		!= VK_SUCCESS) {
		throw std::runtime_error(
			std::string("ERROR: Failed to create an image view in RTSystem."));
	}
	return imageView;
}

//https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceMemoryProperties.html
int32_t RTSystem::getMemoryType(VkImage image) {
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image, &memoryRequirements);
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t ind = 0; ind < memoryProperties.memoryTypeCount; ind++) {
		const uint32_t typeBits = 1 << ind;
		const bool isRequiredMemoryType = memoryRequirements.memoryTypeBits
			& typeBits;
		if (isRequiredMemoryType) return static_cast<uint32_t>(ind);
	}
	throw std::runtime_error("ERROR: Unable to find suitable memory type in RTSystem.");
}

void RTSystem::createStorageImages() {
	VkExtent2D extent;
	extent.width = mainWindow->resolution.first;
	extent.height = mainWindow->resolution.second;
	rtImages.resize(swapChainImages.size());
	rtImageMemorys.resize(swapChainImages.size());
	rtSamplers.resize(swapChainImages.size());

	for (size_t image = 0; image < swapChainImages.size(); image++) {
		createImage(extent.width, extent.height, VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT
			| VK_IMAGE_USAGE_SAMPLED_BIT, 0,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rtImages[image],
			rtImageMemorys[image]);

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; //TODO: make this variable
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.f;
		samplerInfo.minLod = 0;
		samplerInfo.maxLod = 0;

		if (vkCreateSampler(device, &samplerInfo, nullptr, &rtSamplers[image]) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Unable to create a sampler in VulkanSystem.");
		}
	}
}


void RTSystem::createImageViews() {
	swapChainImageViews.resize(swapChainImages.size());
	for (size_t imageIndex = 0; imageIndex < swapChainImages.size();
		imageIndex++) {
		swapChainImageViews[imageIndex] = createImageView(
			swapChainImages[imageIndex], swapChainImageFormat);
	}
	rtImageViews.resize(rtImages.size());
	for (size_t imageIndex = 0; imageIndex < rtImages.size();
		imageIndex++) {
		rtImageViews[imageIndex] = createImageView(
			rtImages[imageIndex], VK_FORMAT_R32G32B32A32_SFLOAT);
	}
}

void RTSystem::AS::create(VkAccelerationStructureCreateInfoKHR createInfo, VkDevice device) {
	//https://github.com/nvpro-samples/nvpro_core/blob/8f7bcbffabf2e031bebc198daf10e196439474a4/nvvk/resourceallocator_vk.cpp#L501
	createInfo.buffer = buf;

	vkCreateAccelerationStructureKHR =
		reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>
		(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));

	vkCreateAccelerationStructureKHR(device,&createInfo,nullptr,&acc);
}

void RTSystem::createBLAccelereationStructure(
	std::vector<uint32_t> meshIndicies,
	std::vector<BuildData>& buildData,
	VkDeviceAddress scratchAddress,
	VkQueryPool queryPool) {
	if (queryPool) {
		vkResetQueryPool(device, queryPool, 0, static_cast<uint32_t>(meshIndicies.size()));
	}
	uint32_t queryCount{ 0 };

	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	for (const uint32_t& idx : meshIndicies) {
		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = buildData[idx].sizeInfo.accelerationStructureSize;

		createBuffer(createInfo.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
			blas[idx].buf, blas[idx].mem, true);
		blas[idx].create(createInfo,device);
		buildData[idx].buildInfo.dstAccelerationStructure = blas[idx].acc;
		buildData[idx].buildInfo.scratchData.deviceAddress = scratchAddress;
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1,
			&buildData[idx].buildInfo, &buildData[idx].rangeInfo);


		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		if (queryPool) {
			vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, 1,
				&buildData[idx].buildInfo.dstAccelerationStructure,
				VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool,
				queryCount++);
		}
	}

	endSingleTimeCommands(commandBuffer);

}

void RTSystem::compactBLAccelereationStructure(
	std::vector<uint32_t> meshIndicies,
	std::vector<BuildData> buildData,
	VkQueryPool queryPool,
	std::vector<AS>& cleanupAs) {
	std::vector<VkDeviceSize> compactSizes(meshIndicies.size());
	vkGetQueryPoolResults(device, queryPool, 0, compactSizes.size(),
		compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), 
		sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);
	size_t queryCount = 0;

	for (uint32_t idx : meshIndicies) {
		cleanupAs.push_back(blas[idx]);
		buildData[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCount++];
		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = buildData[idx].sizeInfo.accelerationStructureSize;
		blas[idx].create(createInfo,device);
		
		VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
		copyInfo.src = buildData[idx].buildInfo.dstAccelerationStructure;
		copyInfo.dst = blas[idx].acc;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();
		vkCmdCopyAccelerationStructureKHR(commandBuffer, &copyInfo);
		endSingleTimeCommands(commandBuffer);
		
	}
}

void RTSystem::createBLAccelereationStructures(uint32_t flags) {
	size_t numMeshes = meshIndexBuffers.size();
	//For geometry
	std::vector< VkAccelerationStructureGeometryKHR> geometries =
		std::vector< VkAccelerationStructureGeometryKHR>(numMeshes);
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> offsets =
		std::vector< VkAccelerationStructureBuildRangeInfoKHR>(numMeshes);

	//For build
	VkDeviceSize accStructTotalSize = { 0 };
	VkDeviceSize maxScratchSize = { 0 };
	uint32_t blasToCompact = 0;
	std::vector<BuildData> buildData = std::vector<BuildData>(numMeshes);

	//Create in terms of meshes
	for (int mesh = 0; mesh < (size_t)numMeshes; mesh++) {
		//Produce geometry
		VkDeviceAddress vertexAddress = getBufferAddress(device, vertexBuffer);
		VkDeviceAddress indexAddress = getBufferAddress(device, meshIndexBuffers[mesh]);
		VkDeviceAddress transformAddress = getBufferAddress(device, meshTransformBuffers[mesh]);
		meshIndexBufferAddresses.push_back(indexAddress);
		uint32_t maxTriCount = indexPoolsMesh[mesh].size() / 3;



		VkAccelerationStructureGeometryTrianglesDataKHR triangles
		{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexStride = sizeof(Vertex);
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = indexAddress;
		triangles.transformData.deviceAddress = transformAddress;
		triangles.maxVertex = meshMinMax[mesh].second;

		geometries[mesh] = VkAccelerationStructureGeometryKHR{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
		};
		geometries[mesh].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometries[mesh].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		geometries[mesh].geometry.triangles = triangles;


		offsets[mesh].firstVertex = 0;
		offsets[mesh].primitiveCount = maxTriCount;
		offsets[mesh].primitiveOffset = 0;
		offsets[mesh].transformOffset = 0;


		//Pre-Build
		buildData[mesh].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildData[mesh].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildData[mesh].buildInfo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | flags;
		buildData[mesh].buildInfo.geometryCount = 1;
		buildData[mesh].buildInfo.pGeometries = &geometries[mesh];
		buildData[mesh].buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildData[mesh].rangeInfo = &offsets[mesh];
		buildData[mesh].sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

		vkGetAccelerationStructureBuildSizesKHR(device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildData[mesh].buildInfo,
			&offsets[mesh].primitiveCount, &buildData[mesh].sizeInfo);

		accStructTotalSize += buildData[mesh].sizeInfo.accelerationStructureSize;
		maxScratchSize = std::max(maxScratchSize,
			buildData[mesh].sizeInfo.buildScratchSize);
		blasToCompact += (buildData[mesh].buildInfo.flags &
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != 0 ? 1 : 0;
	}

	//Create Index Address Buffer
	size_t indexAddressSize = meshIndexBufferAddresses.size() * sizeof(uint64_t);
	int usageBits = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	int propertyBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkBuffer stagingBuffer{};
	VkDeviceMemory stagingBufferMemory{};
	createBuffer(indexAddressSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, propertyBits,
		stagingBuffer, stagingBufferMemory, true);
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, indexAddressSize, 0, &data);
	memcpy(data, meshIndexBufferAddresses.data(), (size_t)indexAddressSize);
	vkUnmapMemory(device, stagingBufferMemory);
	createBuffer(indexAddressSize, usageBits, propertyBits,
		indexAddressBuffer, IndexAddressBufferMemorys, realloc);
	copyBuffer(stagingBuffer, indexAddressBuffer, indexAddressSize);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	
	//Build
	VkBuffer scratchBuffer; 
	VkDeviceMemory scratchBufferMemrory;
	createBuffer(maxScratchSize,  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, scratchBuffer,
		scratchBufferMemrory, true);
	VkDeviceAddress scratchAddress = getBufferAddress(device,scratchBuffer);

	VkQueryPool queryPool{ VK_NULL_HANDLE };
	if (blasToCompact == numMeshes) {
		VkQueryPoolCreateInfo queryInfo{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
		};
		queryInfo.queryCount = numMeshes;
		queryInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(device, &queryInfo, nullptr, &queryPool);
	}

	std::vector<uint32_t> meshIndicies;
	VkDeviceSize batchSize{ 0 };
	VkDeviceSize batchLimit{ 250'000'000 };
	blas.resize(numMeshes);
	for (size_t mesh = 0; mesh < numMeshes; mesh++) {
		meshIndicies.push_back(mesh);
		batchSize += buildData[mesh].sizeInfo.accelerationStructureSize;
		if (batchSize >= batchLimit || mesh >= numMeshes - 1) {
			createBLAccelereationStructure(meshIndicies, buildData, scratchAddress, queryPool);
			if (queryPool) {
				std::vector<AS> cleanupAs;
				compactBLAccelereationStructure(meshIndicies, buildData, queryPool, cleanupAs);
				

				//TODO: Destroy cleanupAS!

				meshIndicies.clear();
				batchSize = 0;
			}
		}
	}
}

void RTSystem::createTLAccelereationStructures(VkBuildAccelerationStructureFlagsKHR flags) {
	//Produce instances
	tlasInstances.resize(blas.size());
	for (int inst = 0; inst < tlasInstances.size(); inst++) {
		tlasInstances[inst].transform = identityVKTransform();
		tlasInstances[inst].instanceCustomIndex = inst;

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addressInfo.accelerationStructure = blas[inst].acc;
		VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

		tlasInstances[inst].accelerationStructureReference = address;
		tlasInstances[inst].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		tlasInstances[inst].mask = 0xFF;
		tlasInstances[inst].instanceShaderBindingTableRecordOffset = 0;
	}

	//Create instance buffer
	VkBuffer accBuffer;
	VkDeviceMemory accMemory;
	VkBuffer scratchBuffer;
	VkDeviceMemory scratchMemory;
	size_t accSize = sizeof(VkAccelerationStructureInstanceKHR) * tlasInstances.size();
	createBuffer(accSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
		accBuffer,accMemory, true);

	void* data;
	vkMapMemory(device, accMemory, 0, accSize, 0, &data);
	memcpy(data, tlasInstances.data(), accSize);
	vkUnmapMemory(device, accMemory);

	VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.buffer = accBuffer;
	VkDeviceAddress accBufferAdr = vkGetBufferDeviceAddress(device, &bufferInfo);
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &barrier, 0, nullptr, 0, nullptr);

	//Preparing geometry
	VkAccelerationStructureGeometryInstancesDataKHR instancesData
	{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instancesData.data.deviceAddress = accBufferAdr;
	VkAccelerationStructureGeometryKHR geometry
	{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instancesData;

	//Sizes
	uint32_t instanceCount = tlasInstances.size();
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.flags = flags;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
		&instanceCount, &sizeInfo);

	//Create tlas
	VkAccelerationStructureCreateInfoKHR createInfo{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR
	};
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createBuffer(createInfo.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
		tlas.buf, tlas.mem, true);
	tlas.create(createInfo, device);

	//Allocate scratch buffer
	createBuffer(sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, scratchBuffer, scratchMemory, 
		true);
	VkBufferDeviceAddressInfo scratchInfo{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO
	};
	scratchInfo.pNext = nullptr;
	scratchInfo.buffer = scratchBuffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &scratchInfo);
	
	//Build tlas
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = tlas.acc;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	VkAccelerationStructureBuildRangeInfoKHR offsetInfo;
	offsetInfo.primitiveCount = instanceCount;
	offsetInfo.firstVertex = 0;
	offsetInfo.primitiveOffset = 0;
	offsetInfo.transformOffset = 0;
	VkAccelerationStructureBuildRangeInfoKHR* offsetInfoP = &offsetInfo;
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &offsetInfoP);

	endSingleTimeCommands(commandBuffer);
	//TODO: destroy scratch buffer
	//TODO: destory acc buffer
}

void RTSystem::createAccelereationStructures() {
	//https://github.com/SaschaWillems/Vulkan/blob/master/examples/raytracingbasic/raytracingbasic.cpp

	vkGetAccelerationStructureBuildSizesKHR = 
		reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>
		(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
	vkCmdWriteAccelerationStructuresPropertiesKHR = 
		reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>
		(vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
	vkCmdCopyAccelerationStructureKHR =
		reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>
		(vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR"));
	vkCmdBuildAccelerationStructuresKHR =
		reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>
		(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	vkGetBufferDeviceAddress =
		reinterpret_cast<PFN_vkGetBufferDeviceAddress>
		(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
	vkGetAccelerationStructureDeviceAddressKHR =
		reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>
		(vkGetDeviceProcAddr(device,"vkGetAccelerationStructureDeviceAddressKHR"));
	vkCmdTraceRaysKHR =
		reinterpret_cast<PFN_vkCmdTraceRaysKHR>
		(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

	createBLAccelereationStructures(VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	createTLAccelereationStructures();
}

void RTSystem::createRenderPasses() {


	VkSubpassDescription offscreenSubpass = {};

	VkAttachmentDescription colorAttachmentOffscreen{};
	colorAttachmentOffscreen.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	colorAttachmentOffscreen.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentOffscreen.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentOffscreen.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentOffscreen.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentOffscreen.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentOffscreen.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	colorAttachmentOffscreen.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRefOffscreen{};
	colorAttachmentRefOffscreen.attachment = 0;
	colorAttachmentRefOffscreen.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;



	offscreenSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	offscreenSubpass.colorAttachmentCount = 1;
	offscreenSubpass.pColorAttachments = &colorAttachmentRefOffscreen;
	offscreenSubpass.inputAttachmentCount = 0;
	offscreenSubpass.pInputAttachments = nullptr;

	//Dependency for offscreen pass
	VkSubpassDependency offscreenDependency = {};
	offscreenDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	offscreenDependency.dstSubpass = 0;
	offscreenDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	offscreenDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	offscreenDependency.srcAccessMask = 0;
	offscreenDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> offscreenAttachments;
	offscreenAttachments.push_back(colorAttachmentOffscreen);
	VkRenderPassCreateInfo offscreenRenderPassInfo{};
	offscreenRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	offscreenRenderPassInfo.attachmentCount = offscreenAttachments.size();
	offscreenRenderPassInfo.pAttachments = offscreenAttachments.data();
	offscreenRenderPassInfo.subpassCount = 1;
	offscreenRenderPassInfo.pSubpasses = &offscreenSubpass;
	offscreenRenderPassInfo.dependencyCount = 1;
	offscreenRenderPassInfo.pDependencies = &offscreenDependency;

	if (vkCreateRenderPass(device, &offscreenRenderPassInfo, nullptr, &offscreenPass) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Was unable to create render pass in VulkanSystem.");
	}




	//Final renderpass
	VkSubpassDescription finalSubpass = {};

	VkAttachmentDescription colorAttachmentFinal{};
	colorAttachmentFinal.format = swapChainImageFormat;
	colorAttachmentFinal.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentFinal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentFinal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentFinal.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentFinal.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentFinal.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentFinal.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRefFinal{};
	colorAttachmentRefFinal.attachment = 0;
	colorAttachmentRefFinal.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;



	finalSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	finalSubpass.colorAttachmentCount = 1;
	finalSubpass.pColorAttachments = &colorAttachmentRefFinal;
	finalSubpass.inputAttachmentCount = 0;
	finalSubpass.pInputAttachments = nullptr;

	//Dependency for final pass
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


	std::vector<VkAttachmentDescription> attachments;
	attachments.push_back(colorAttachmentFinal);
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &finalSubpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Was unable to create render pass in VulkanSystem.");
	}

}

void RTSystem::createDescriptorSetLayout() {

	descriptorSetLayouts.resize(2);

	VkDescriptorSetLayoutBinding tlasBinding{};
	tlasBinding.binding = 0;
	tlasBinding.descriptorCount = 1;
	tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlasBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	tlasBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outImageBinding{};
	outImageBinding.binding = 1;
	outImageBinding.descriptorCount = 1;
	outImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	outImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	outImageBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cameraBinding{};
	cameraBinding.binding = 2;
	cameraBinding.descriptorCount = 1;
	cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding persBinding{};
	persBinding.binding = 3;
	persBinding.descriptorCount = 1;
	persBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	persBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding verticesBit{};
	verticesBit.binding = 4;
	verticesBit.descriptorCount = 1;
	verticesBit.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	verticesBit.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	VkDescriptorSetLayoutBinding indicesBit{};
	indicesBit.binding = 5;
	indicesBit.descriptorCount = 1;
	indicesBit.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indicesBit.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;



	VkDescriptorSetLayoutBinding materialBinding{};
	materialBinding.binding = 6;
	materialBinding.descriptorCount = 1;
	materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	VkDescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 7;
	textureBinding.descriptorCount = rawTextures.size();
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBinding.pImmutableSamplers = nullptr;
	textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;


	VkDescriptorSetLayoutBinding bindings[] = {tlasBinding, outImageBinding, 
		cameraBinding, persBinding, verticesBit, indicesBit, materialBinding, 
		textureBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 8;
	layoutInfo.pBindings = bindings;
	if (vkCreateDescriptorSetLayout(
		device, &layoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failed to create a descriptor set layout in Vulkan System.");
	}

	VkDescriptorSetLayoutBinding hdrBinding{};
	hdrBinding.binding = 0;
	hdrBinding.descriptorCount = 1;
	hdrBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	hdrBinding.pImmutableSamplers = nullptr;
	hdrBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


	VkDescriptorSetLayoutCreateInfo layoutInfoFinal{};
	layoutInfoFinal.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfoFinal.bindingCount = 1;
	layoutInfoFinal.pBindings = &hdrBinding;
	if (vkCreateDescriptorSetLayout(
		device, &layoutInfoFinal, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failed to create a descriptor set layout in Vulkan System.");
	}


}


void RTSystem::createRTPipeline(
	std::string raygen,
	std::string miss,
	std::string closestHit,
	VkPipeline& pipeline,
	VkPipelineLayout& layout,
	int subpass,
	VkRenderPass inRenderPass
) {

	std::vector<char> raygenShaderRawData = readFile((shaderDir + raygen).c_str());
	std::vector<char> missShaderRawData = readFile((shaderDir + miss).c_str());
	std::vector<char> closestHitShaderRawData = readFile((shaderDir + closestHit).c_str());
	VkShaderModule raygenShaderModule = createShaderModule(raygenShaderRawData);
	VkShaderModule missShaderModule = createShaderModule(missShaderRawData);
	VkShaderModule closestHitShaderModule = createShaderModule(closestHitShaderRawData);

	std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
	for (int i = 0; i < 3; i++) {
		stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[i].pName = "main";
	}
	stages[0].module = raygenShaderModule;
	stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[1].module = missShaderModule;
	stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[2].module = closestHitShaderModule;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	//Shader group
	for (int i = 0; i < 3; i++) {
		shaderGroups[i].sType =
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroups[i].anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroups[i].closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroups[i].generalShader = VK_SHADER_UNUSED_KHR;
		shaderGroups[i].intersectionShader = VK_SHADER_UNUSED_KHR;
	}
	shaderGroups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[0].generalShader = 0;
	shaderGroups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[1].generalShader = 1;
	shaderGroups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroups[2].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[2].closestHitShader = 2;

	VkPushConstantRange pushConstant{ VK_SHADER_STAGE_RAYGEN_BIT_KHR | 
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
		0, sizeof(PushConstantRay) };


	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ 
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO 
	};
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts[0];
	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create rt pipeline layout in  RTSystem.");
	}
	VkRayTracingPipelineCreateInfoKHR pipelineInfo{
		VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR
	};
	pipelineInfo.stageCount = stages.size();
	pipelineInfo.pStages = stages.data();
	pipelineInfo.groupCount = shaderGroups.size();
	pipelineInfo.pGroups = shaderGroups.data();
	pipelineInfo.maxPipelineRayRecursionDepth = 1;
	pipelineInfo.layout = layout;
	if (vkCreateRayTracingPipelinesKHR(device, {}, {}, 1,
		&pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to make ray tracing pipelines in RTSystem.");
	}

	vkDestroyShaderModule(device, closestHitShaderModule, nullptr);
	vkDestroyShaderModule(device, missShaderModule, nullptr);
	vkDestroyShaderModule(device, raygenShaderModule, nullptr);
}

void RTSystem::createGraphicsPipeline(std::string vertShader,
	std::string fragShader, VkPipeline& pipeline, VkPipelineLayout& layout,
	int subpass, VkRenderPass inRenderPass) {
	std::vector<char> vertexShaderRawData = readFile((shaderDir + vertShader).c_str());
	std::vector<char> fragmentShaderRawData = readFile((shaderDir + fragShader).c_str());

	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderRawData);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderRawData);

	VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
	vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageInfo.module = vertexShaderModule;
	vertexShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
	fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageInfo.module = fragmentShaderModule;
	fragmentShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	std::array<VkVertexInputAttributeDescription, 6>
		attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();


	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = false;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //Anything else requires GPU feature to be enabled
	rasterizer.lineWidth = 1.0f; //NEEDS wideLines to be enabled if > 1.0
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = false; //Add to depth value, not using

	//Currently not multisampling, will enable later
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = true;
	depthStencil.depthWriteEnable = true;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;


	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = inRenderPass;
	pipelineInfo.subpass = subpass;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create graphics pipeline in RTSystem.");
	}

	vkDestroyShaderModule(device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

}

void RTSystem::createGraphicsPipelines() {
	vkCreateRayTracingPipelinesKHR =
		reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>
		(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

	createRTPipeline("/rayGen.spv", "/miss.spv", "/closestHit.spv", 
		graphicsPipelineRT, pipelineLayoutRT, 0, offscreenPass);

	VkPipelineLayoutCreateInfo pipelineLayoutInfoFinal{};
	pipelineLayoutInfoFinal.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfoFinal.setLayoutCount = 1;
	pipelineLayoutInfoFinal.pSetLayouts = &descriptorSetLayouts[1];
	pipelineLayoutInfoFinal.pushConstantRangeCount = 0;
	pipelineLayoutInfoFinal.pPushConstantRanges = nullptr;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfoFinal, nullptr, &pipelineLayoutFinal) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create pipeline layout in RTSystem.");
	}
	createGraphicsPipeline("/vertQuad.spv", "/rtFinal.spv", graphicsPipelineFinal, pipelineLayoutFinal, 0, renderPass);

}



VkShaderModule RTSystem::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Failed to create a shader module in RTSystem.");
	}
	return shaderModule;
}

void RTSystem::createShaderBindingTable() {
	vkGetRayTracingShaderGroupHandlesKHR = 
		reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>
		(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));


	//Define shader group information
	auto align = [](auto size, auto alignment) {
		return (size + alignment - 1) & (~(alignment - 1));
	};
	uint32_t missCount = 1;
	uint32_t hitCount = 1;
	uint32_t raygenCount = 1; //Always 1
	uint32_t handleCount = missCount + hitCount + raygenCount;
	uint32_t handleSize = rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;
	uint32_t handleSizeAligned = align(handleSize, handleAlignment);
	rgenRegion.stride = align(handleSizeAligned, baseAlignment);
	rgenRegion.size = rgenRegion.stride;
	missRegion.stride = handleSizeAligned;
	missRegion.size = align(missCount * handleSizeAligned, baseAlignment);
	hitRegion.stride = handleSizeAligned;
	hitRegion.size = align(hitCount * handleSizeAligned, baseAlignment);

	uint32_t handleDataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(handleDataSize);
	if (vkGetRayTracingShaderGroupHandlesKHR(device, graphicsPipelineRT,
		0, handleCount, handleDataSize, handles.data()) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to get rt shader group handles in RTSystem.");
	}

	//Create buffer
	VkDeviceSize bufferSize =
		rgenRegion.size +
		missRegion.size +
		hitRegion.size +
		callRegion.size;
	createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sbtBuffer, sbtMemory, true);

	VkBufferDeviceAddressInfo addressInfo{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,nullptr,sbtBuffer
	};
	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &addressInfo);
	rgenRegion.deviceAddress = sbtAddress;
	missRegion.deviceAddress = rgenRegion.deviceAddress + rgenRegion.size;
	hitRegion.deviceAddress = missRegion.deviceAddress + missRegion.size;

	//Helper function as recomended by the nvpro tutorial
	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	void* data;
	vkMapMemory(device, sbtMemory, 0, bufferSize, 0, &data);
	uint8_t* sbtData = reinterpret_cast<uint8_t*>(data);

	uint8_t* variableData{ nullptr };
	uint32_t handleIndex = 0;
	variableData = sbtData;
	memcpy(variableData, getHandle(handleIndex), handleSize);
	handleIndex++;
	variableData = sbtData + rgenRegion.size;
	for (uint32_t miss = 0; miss < missCount; miss++) {
		memcpy(variableData, getHandle(handleIndex), handleSize);
		variableData += missRegion.stride;
		handleIndex++;
	}
	variableData = sbtData + rgenRegion.size + missRegion.size;
	for (uint32_t hit = 0; hit < hitCount; hit++) {
		memcpy(variableData, getHandle(handleIndex), handleSize);
		variableData += hitRegion.stride;
		handleIndex++;
	}

	vkUnmapMemory(device, sbtMemory);

}

void RTSystem::createFramebuffers() {
	swapChainFramebuffers.resize(swapChainImageViews.size());
	for (size_t image = 0; image < swapChainImageViews.size(); image++) {
		std::vector<VkImageView> attachments;
		attachments.push_back(swapChainImageViews[image]);
		
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;
		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
			&(swapChainFramebuffers[image])) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Unable to create a framebuffer in RTSystem.");
		}

		attachments.clear();

	}
}

void RTSystem::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, VkBuffer& buffer,
	VkDeviceMemory& bufferMemory, bool realloc) {
	if (size == 0) return;
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (realloc)
	{
		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Creating a buffer in RTSystem.");
		}
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	VkMemoryAllocateFlagsInfo flags{};
	flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
		properties, physicalDevice);
	allocInfo.pNext = &flags;
	if (realloc) {
		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Unable to allocate a buffer memory in RTSystem.");
		}
		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}
}

VkCommandBuffer RTSystem::beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void RTSystem::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void RTSystem::copyBuffer(VkBuffer source, VkBuffer dest,
	VkDeviceSize size) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, source, dest, 1, &copyRegion);
	endSingleTimeCommands(commandBuffer);
}

void RTSystem::createVertexBuffer(bool realloc) {
	useVertexBuffer = false;
	int stagingBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	int vertexUsageBits = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	int vertexPropertyBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	if (vertices.size() != 0) {
		useVertexBuffer = true;

		//Create temp staging buffer
		size_t vertSize = sizeof(vertices[0]);
		VkDeviceSize bufferSize = vertSize * vertices.size();
		VkBuffer stagingBuffer{};
		VkDeviceMemory stagingBufferMemory{};
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBits,
			stagingBuffer, stagingBufferMemory, true);

		//Move vertex data to GPU
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		//Create proper vertex buffer
		createBuffer(bufferSize, vertexUsageBits, vertexPropertyBits,
			vertexBuffer, vertexBufferMemory, realloc);
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}
}

void RTSystem::createTransformBuffers(bool realloc) {

	int stagingBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	int usageBits = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	int propertyBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	meshTransformBuffers.resize(transformPoolsMesh.size());
	meshTransformMemorys.resize(transformPoolsMesh.size());
	for (int mesh = 0; mesh < transformPoolsMesh.size(); mesh++) {
		//Convert transform to vulkan format
		VkTransformMatrixKHR transform = transformPoolsMesh[mesh].toVulkan();

		//Create temp staging buffer
		VkDeviceSize bufferSize = sizeof(VkTransformMatrixKHR);
		VkBuffer stagingBuffer{};
		VkDeviceMemory stagingBufferMemory{};
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBits,
			stagingBuffer, stagingBufferMemory, true);

		//Move vertex data to GPU
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, &transform, (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		//Create proper vertex buffer
		createBuffer(bufferSize, usageBits, propertyBits,
			meshTransformBuffers[mesh], meshTransformMemorys[mesh], realloc);
		copyBuffer(stagingBuffer, meshTransformBuffers[mesh], bufferSize);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}
}

mat44<float> RTSystem::getCameraSpace(DrawCamera camera, float_3 useMoveVec, float_3 useDirVec) {
	useDirVec = useDirVec.normalize() * -1;
	float_3 up = float_3(0, 0, 1);
	float_3 cameraRight = up.cross(useDirVec);
	float_3 cameraUp = useDirVec.cross(cameraRight);
	cameraRight = cameraRight.normalize(); cameraUp = cameraUp.normalize();
	vec4 transposed0 = float_4(cameraRight[0], cameraUp[0], useDirVec[0], 0).normalize();
	vec4 transposed1 = float_4(cameraRight[1], cameraUp[1], useDirVec[1], 0).normalize();
	vec4 transposed2 = float_4(cameraRight[2], cameraUp[2], useDirVec[2], 0).normalize();
	mat44 localRot = mat44<float>(transposed0, transposed1, transposed2, float_4(0, 0, 0, 1));
	float_3 moveLocal = float_3(useMoveVec.x, useMoveVec.y, useMoveVec.z);
	mat44 local = mat44<float>(localRot);
	local.data[3][0] = moveLocal.x;
	local.data[3][1] = moveLocal.y;
	local.data[3][2] = moveLocal.z;
	local = local * camera.transform;
	return local;
}

mat44<float> RTSystem::getInvCameraSpace(DrawCamera camera, float_3 useMoveVec, float_3 useDirVec) {
	useDirVec = useDirVec.normalize();
	float_3 up = float_3(0, 0, 1);
	float_3 cameraRight = up.cross(useDirVec);
	float_3 cameraUp = useDirVec.cross(cameraRight);
	cameraRight = cameraRight.normalize(); cameraUp = cameraUp.normalize();
	vec4 transposed0 = float_4(cameraRight[0], cameraUp[0], useDirVec[0], 0).normalize();
	vec4 transposed1 = float_4(cameraRight[1], cameraUp[1], useDirVec[1], 0).normalize();
	vec4 transposed2 = float_4(cameraRight[2], cameraUp[2], useDirVec[2], 0).normalize();
	mat44 localRot = mat44<float>(transposed0, transposed1, transposed2, float_4(0, 0, 0, 1));
	float_3 moveLocal = float_3(-useMoveVec.x, -useMoveVec.y, -useMoveVec.z);
	mat44 local = mat44<float>(localRot);
	local.data[3][0] = moveLocal.x;
	local.data[3][1] = moveLocal.y;
	local.data[3][2] = moveLocal.z;
	local = camera.invTransform * local;
	return local;
}


void RTSystem::transitionImageLayout(VkImage image, VkFormat format,
	VkImageLayout oldLayout, VkImageLayout newLayout, int layers, int levels) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = levels;
	barrier.subresourceRange.layerCount = layers;


	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destStage;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout
		== VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout
		== VK_IMAGE_LAYOUT_GENERAL) {
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout
		== VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout
		== VK_IMAGE_LAYOUT_GENERAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout
		== VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
		destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		throw std::runtime_error("ERROR: Invalid arguments passed into layout transition in RTSystem.");
	}


	vkCmdPipelineBarrier(commandBuffer, sourceStage, destStage, 0, 0,
		nullptr, 0, nullptr, 1, &barrier);

	endSingleTimeCommands(commandBuffer);
}


void RTSystem::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int level, int face) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = level;
	region.imageSubresource.baseArrayLayer = face;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0,0,0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	endSingleTimeCommands(commandBuffer);
}

//https://vulkan-tutorial.com/Generating_Mipmaps
void RTSystem::generateMipmaps(VkImage image, int32_t x, int32_t y, uint32_t mipLevels, int face) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = face;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int mipX = x; int mipY = y;
	for (int mipLevel = 1; mipLevel < mipLevels; mipLevel++) {
		int lastMipLevel = mipLevel - 1;
		barrier.subresourceRange.baseMipLevel = lastMipLevel;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
			&barrier);

		//Blit mipmap
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0,0,0 };
		blit.srcOffsets[1] = { mipX,mipY,1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = lastMipLevel;
		blit.srcSubresource.baseArrayLayer = face;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = {
			mipX > 1 ? mipX / 2 : 1,
			mipY > 1 ? mipY / 2 : 1,
			1
		};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = mipLevel;
		blit.dstSubresource.baseArrayLayer = face;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);


		if (mipX > 1) mipX /= 2; if (mipY > 1) mipY /= 2;
	}
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&barrier);

	endSingleTimeCommands(commandBuffer);
}


void RTSystem::createEnvironmentImage(Texture env, VkImage& image,
	VkDeviceMemory& memory, VkImageView& imageView, VkSampler& sampler) {

	VkDeviceSize imageSize = 4 * env.x * env.realY;
	VkDeviceSize layerSize = 4 * env.x * env.y;
	std::array<VkBuffer, 6> stageBuffer;
	std::array<VkDeviceMemory, 6> stageBufferMemory;
	for (int face = 0; face < 6; face++) {
		createBuffer(imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stageBuffer[face], stageBufferMemory[face], true);
		void* data;
		vkMapMemory(device, stageBufferMemory[face], 0, layerSize, 0, &data);
		memcpy(data, (void*)(&env.data[face * static_cast<size_t>(layerSize)]), static_cast<size_t>(layerSize));
		vkUnmapMemory(device, stageBufferMemory[face]);
	}
	if (env.doFree) {
		stbi_image_free((void*)env.data);
	}

	int usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	int flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	createImage(env.x, env.y, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL, usage, flags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory, 6, env.mipLevels);
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, env.mipLevels);
	for (int face = 0; face < 6; face++) {
		copyBufferToImage(stageBuffer[face], image, static_cast<uint32_t>(env.x), static_cast<uint32_t>(env.y), 0, face);
		generateMipmaps(image, env.x, env.y, env.mipLevels, face);
		vkDestroyBuffer(device, stageBuffer[face], nullptr);
		vkFreeMemory(device, stageBufferMemory[face], nullptr);
	}


	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.layerCount = 6;
	viewInfo.subresourceRange.levelCount = env.mipLevels;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a texture image view in RTSystem.");
	}


	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; //TODO: make this variable
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = env.mipLevels;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a sampler in RTSystem.");
	}
	int a = 0;
}

void RTSystem::createTextureImage(Texture tex, VkImage& image,
	VkDeviceMemory& memory, VkImageView& imageView, VkSampler& sampler) {
	VkDeviceSize imageSize = 4 * tex.x * tex.realY;
	VkBuffer stageBuffer;
	VkDeviceMemory stageBufferMemory;
	createBuffer(imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stageBuffer, stageBufferMemory, true);
	void* data;
	vkMapMemory(device, stageBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, (void*)tex.data, static_cast<size_t>(imageSize));
	vkUnmapMemory(device, stageBufferMemory);
	if (tex.doFree) {
		stbi_image_free((void*)tex.data);
	}

	int usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	createImage(tex.x, tex.realY, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL, usage, 0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory, 1, tex.mipLevels);
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, tex.mipLevels);
	copyBufferToImage(stageBuffer, image, static_cast<uint32_t>(tex.x), static_cast<uint32_t>(tex.realY));
	generateMipmaps(image, tex.x, tex.realY, tex.mipLevels);

	vkDestroyBuffer(device, stageBuffer, nullptr);
	vkFreeMemory(device, stageBufferMemory, nullptr);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.levelCount = tex.mipLevels;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a texture image view in RTSystem.");
	}

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; //TODO: make this variable
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = static_cast<float>(tex.mipLevels);

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a sampler in RTSystem.");
	}
}

void RTSystem::createTextureImages() {
	textureImages.resize(rawTextures.size());
	textureImageMemorys.resize(rawTextures.size());
	textureImageViews.resize(rawTextures.size());
	textureSamplers.resize(rawTextures.size());
	for (size_t texInd = 0; texInd < rawTextures.size(); texInd++) {
		Texture tex = rawTextures[texInd];
		createTextureImage(
			tex,
			textureImages[texInd],
			textureImageMemorys[texInd],
			textureImageViews[texInd],
			textureSamplers[texInd]
		);
	}
	createTextureImage(LUT, LUTImage, LUTImageMemory, LUTImageView, LUTSampler);
	cubeImages.resize(rawCubes.size());
	cubeImageMemorys.resize(rawCubes.size());
	cubeImageViews.resize(rawCubes.size());
	cubeSamplers.resize(rawCubes.size());
	for (size_t cubeInd = 0; cubeInd < rawCubes.size(); cubeInd++) {
		Texture tex = rawCubes[cubeInd];
		createEnvironmentImage(
			tex,
			cubeImages[cubeInd],
			cubeImageMemorys[cubeInd],
			cubeImageViews[cubeInd],
			cubeSamplers[cubeInd]
		);
	}
	if (rawEnvironment.has_value()) {
		createEnvironmentImage(
			*rawEnvironment,
			environmentImage,
			environmentImageMemory,
			environmentImageView,
			environmentSampler
		);
	}
	int a = 0;

}


void RTSystem::createIndexBuffers(bool realloc, bool andFree) {
	if (andFree) {
		for (int pool = 0; pool < meshIndexBufferMemorys.size(); pool++) {
			vkDestroyBuffer(device, meshIndexBuffers[pool], nullptr);
			vkFreeMemory(device, meshIndexBufferMemorys[pool], nullptr);
		}
	}

	meshIndexBufferMemorys.resize(indexPoolsMesh.size());
	meshIndexBuffers.resize(indexPoolsMesh.size());
	for (size_t pool = 0; pool < indexPoolsMesh.size(); pool++) {
		VkDeviceSize bufferSize = sizeof(indexPoolsMesh[pool][0]) * indexPoolsMesh[pool].size();
		if (bufferSize > 0) {

			//Create temp staging buffer
			VkBuffer stagingBuffer{};
			VkDeviceMemory stagingBufferMemory{};
			int stagingBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBits,
				stagingBuffer, stagingBufferMemory, true);

			//Move index data to GPU
			void* data;
			vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, indexPoolsMesh[pool].data(), (size_t)bufferSize);
			vkUnmapMemory(device, stagingBufferMemory);

			//Create proper index buffer
			int indexUsageBits = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
				| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			int indexPropertyBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			createBuffer(bufferSize, indexUsageBits, indexPropertyBits,
				meshIndexBuffers[pool], meshIndexBufferMemorys[pool], realloc);
			copyBuffer(stagingBuffer, meshIndexBuffers[pool], bufferSize);

			vkDestroyBuffer(device, stagingBuffer, nullptr);
			vkFreeMemory(device, stagingBufferMemory, nullptr);
		}
	}
}

void RTSystem::createUniformBuffers(bool realloc) {
	uniformBuffersCamera.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMappedCamera.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemoryCamera.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersProj.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMappedProj.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemoryProj.resize(MAX_FRAMES_IN_FLIGHT);

	int props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		VkDeviceSize bufferSizeCameras = sizeof(mat44<float>);
		createBuffer(bufferSizeCameras, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, props,
			uniformBuffersCamera[frame], uniformBuffersMemoryCamera[frame], realloc);
		vkMapMemory(device, uniformBuffersMemoryCamera[frame], 0, bufferSizeCameras, 0,
			&uniformBuffersMappedCamera[frame]);
		createBuffer(bufferSizeCameras, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, props,
			uniformBuffersProj[frame], uniformBuffersMemoryProj[frame], realloc);
		vkMapMemory(device, uniformBuffersMemoryProj[frame], 0, bufferSizeCameras, 0,
			&uniformBuffersMappedProj[frame]);
	}

	//Storage buffers

	size_t matSize = meshMaterials.size() * sizeof(DrawMaterial);
	int usageBits = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	int propertyBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkBuffer stagingBuffer{};
	VkDeviceMemory stagingBufferMemory{};
	createBuffer(matSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, propertyBits,
		stagingBuffer, stagingBufferMemory, true);
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, matSize, 0, &data);
	memcpy(data, meshMaterials.data(), (size_t)matSize);
	vkUnmapMemory(device, stagingBufferMemory);
	createBuffer(matSize, usageBits, propertyBits,
		materialBuffer, materialBufferMemorys, realloc);
	copyBuffer(stagingBuffer, materialBuffer, matSize);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void RTSystem::createDescriptorPool() {

	VkDescriptorPoolSize poolSizeHDR{};
	poolSizeHDR.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizeHDR.descriptorCount = MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPoolCreateInfo poolInfoHDR{};
	poolInfoHDR.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfoHDR.poolSizeCount = 1;
	poolInfoHDR.pPoolSizes = &poolSizeHDR;
	poolInfoHDR.maxSets = MAX_FRAMES_IN_FLIGHT;
	if (vkCreateDescriptorPool(device, &poolInfoHDR, nullptr, &descriptorPoolHDR)
		!= VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a descriptor pool in Vulkan System.");
	}
	VkDescriptorPoolSize poolSizeFinal{};
	poolSizeFinal.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizeFinal.descriptorCount = 1;
	VkDescriptorPoolCreateInfo poolInfoFinal{};
	poolInfoFinal.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfoFinal.poolSizeCount = 1;
	poolInfoFinal.pPoolSizes = &poolSizeFinal;
	poolInfoFinal.maxSets = MAX_FRAMES_IN_FLIGHT;
	if (vkCreateDescriptorPool(device, &poolInfoFinal, nullptr, &descriptorPoolFinal)
		!= VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a descriptor pool in Vulkan System.");
	}
}


void RTSystem::createDescriptorSets() {





	std::vector<VkDescriptorSetLayout> layoutsHDR(MAX_FRAMES_IN_FLIGHT, descriptorSetLayouts[0]);
	VkDescriptorSetAllocateInfo allocateInfoHDR{};
	allocateInfoHDR.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfoHDR.descriptorPool = descriptorPoolHDR;
	allocateInfoHDR.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocateInfoHDR.pSetLayouts = layoutsHDR.data();
	descriptorSetsHDR.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocateInfoHDR, descriptorSetsHDR.data())
		!= VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create descriptor sets in Vulkan System. HDR.");
	}

	std::vector<VkDescriptorSetLayout> layoutsFinal(MAX_FRAMES_IN_FLIGHT, descriptorSetLayouts[1]);
	VkDescriptorSetAllocateInfo allocateInfoFinal{};
	allocateInfoFinal.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfoFinal.descriptorPool = descriptorPoolFinal;
	allocateInfoFinal.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocateInfoFinal.pSetLayouts = layoutsFinal.data();
	descriptorSetsFinal.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocateInfoFinal, descriptorSetsFinal.data())
		!= VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create descriptor sets in Vulkan System. Final.");
	}


	size_t pool = 0;
	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		
		VkWriteDescriptorSetAccelerationStructureKHR tlasDescriptor{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		tlasDescriptor.accelerationStructureCount = 1;
		tlasDescriptor.pAccelerationStructures = &tlas.acc;
		VkDescriptorImageInfo imageDescriptor{ {}, rtImageViews[frame], VK_IMAGE_LAYOUT_GENERAL};

		VkDescriptorBufferInfo bufferInfoCameras{};
		bufferInfoCameras.buffer = uniformBuffersCamera[frame];
		bufferInfoCameras.offset = 0;
		bufferInfoCameras.range = sizeof(mat44<float>);

		VkDescriptorBufferInfo bufferInfoPers{};
		bufferInfoPers.buffer = uniformBuffersProj[frame];
		bufferInfoPers.offset = 0;
		bufferInfoPers.range = sizeof(mat44<float>);

		VkDescriptorBufferInfo bufferInfoVertices{};
		bufferInfoVertices.buffer = vertexBuffer;
		bufferInfoVertices.offset = 0;
		bufferInfoVertices.range = sizeof(Vertex) *vertices.size();

		VkDescriptorBufferInfo bufferInfoIndices{};
		bufferInfoIndices.buffer = indexAddressBuffer;
		bufferInfoIndices.offset = 0;
		bufferInfoIndices.range = sizeof(uint64_t) * meshIndexBufferAddresses.size();

		VkDescriptorBufferInfo bufferInfoMaterials{};
		bufferInfoMaterials.buffer = materialBuffer;
		bufferInfoMaterials.offset = 0;
		bufferInfoMaterials.range = sizeof(DrawMaterial) * meshMaterials.size();

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = std::vector<VkWriteDescriptorSet>(7 + rawTextures.size());
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[0].pNext = &tlasDescriptor;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].dstArrayElement = 0;
		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeDescriptorSets[1].descriptorCount = 1;
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].pImageInfo = &imageDescriptor;
		writeDescriptorSets[1].dstArrayElement = 0;
		writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[2].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[2].descriptorCount = 1;
		writeDescriptorSets[2].dstBinding = 2;
		writeDescriptorSets[2].pBufferInfo = &bufferInfoCameras;
		writeDescriptorSets[2].dstArrayElement = 0;
		writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[3].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[3].descriptorCount = 1;
		writeDescriptorSets[3].dstBinding = 3;
		writeDescriptorSets[3].pBufferInfo = &bufferInfoPers;
		writeDescriptorSets[3].dstArrayElement = 0;
		writeDescriptorSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[4].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[4].descriptorCount = 1;
		writeDescriptorSets[4].dstBinding = 4;
		writeDescriptorSets[4].pBufferInfo = &bufferInfoVertices;
		writeDescriptorSets[4].dstArrayElement = 0;
		writeDescriptorSets[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[5].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[5].descriptorCount = 1;
		writeDescriptorSets[5].dstBinding = 5;
		writeDescriptorSets[5].pBufferInfo = &bufferInfoIndices;
		writeDescriptorSets[5].dstArrayElement = 0;
		writeDescriptorSets[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[6].dstSet = descriptorSetsHDR[frame];
		writeDescriptorSets[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[6].descriptorCount = 1;
		writeDescriptorSets[6].dstBinding = 6;
		writeDescriptorSets[6].pBufferInfo = &bufferInfoMaterials;
		writeDescriptorSets[6].dstArrayElement = 0;
		std::vector<VkDescriptorImageInfo> imageInfosTex = std::vector<VkDescriptorImageInfo>(rawTextures.size());
		for (size_t tex = 0; tex < rawTextures.size(); tex++) {
			size_t descSet = tex + 7;
			imageInfosTex[tex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfosTex[tex].imageView = textureImageViews[tex];
			imageInfosTex[tex].sampler = textureSamplers[tex];
			writeDescriptorSets[descSet].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[descSet].dstSet = descriptorSetsHDR[frame];
			writeDescriptorSets[descSet].dstBinding = 7;
			writeDescriptorSets[descSet].dstArrayElement = tex;
			writeDescriptorSets[descSet].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[descSet].descriptorCount = 1;
			writeDescriptorSets[descSet].pImageInfo = &imageInfosTex[tex];
		}
		
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

	}

	for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		VkDescriptorImageInfo finalDescriptor{};
		finalDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		finalDescriptor.imageView = rtImageViews[frame];
		finalDescriptor.sampler = rtSamplers[frame];

		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSetsFinal[frame];
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.pImageInfo = &finalDescriptor;
		writeDescriptorSet.dstArrayElement = 0;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}
}

void RTSystem::createCommands() {
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = familyIndices.graphicsFamily.value();
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a command pool in RTSystem.");
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = commandBuffers.size();
	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create a command buffer in RTSystem.");
	}



}

void RTSystem::createImage(uint32_t width, uint32_t height, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkMemoryPropertyFlags properties,
	VkImage& image, VkDeviceMemory& imageMemory, int layers, int levels) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = levels;
	imageInfo.arrayLayers = layers;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.flags = flags;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to create an image in RTSystem.");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);
	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to allocate an image memory in Vulkan System!");
	}
	vkBindImageMemory(device, image, imageMemory, 0);
}

void RTSystem::raytrace(VkCommandBuffer commandBuffer) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to begin recording a command buffer in RTSystem.");
	}

	

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, graphicsPipelineRT);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayoutRT, 0,
		1, &descriptorSetsHDR[currentFrame], 0, nullptr);
	vkCmdPushConstants(commandBuffer, pipelineLayoutRT,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
		0, sizeof(PushConstantRay), &pushConstantRT);
	vkCmdTraceRaysKHR(commandBuffer, &rgenRegion, &missRegion, &hitRegion, &callRegion, swapChainExtent.width, swapChainExtent.height, 1);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to record command buffer in RTSystem.");
	}



	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = commandBuffers.data() + currentFrame;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	vkQueueWaitIdle(graphicsQueue);

}

void RTSystem::recordCommandBufferMain(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to begin recording a command buffer in RTSystem.");
	}

	//Begin preparing command buffer render pass
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0,0 };
	renderPassInfo.renderArea.extent = swapChainExtent;
	std::vector<VkClearValue> clearColors;
	clearColors.push_back({ {0.0f, 0.0f, 0.0f, 1.0f} });
	clearColors.push_back({ {1.0f,0} });
	clearColors.push_back({ {0.0f, 0.0f, 0.0f, 1.0f} });

	renderPassInfo.clearValueCount = clearColors.size();
	renderPassInfo.pClearValues = clearColors.data();


	VkViewport viewport{};
	VkRect2D scissor{};
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	//Viewport and Scissor are dyanmic, so set now
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	//Present subpass
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineFinal);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutFinal, 0, 1, &descriptorSetsFinal[currentFrame], 0, NULL);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0); //Draw a quad in shader

	//END render pass
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("ERROR: Unable to record command buffer in RTSystem.");
	}

	
}



void RTSystem::updateUniformBuffers(uint32_t frame) {
	
	//Guided by glm implementation of lookAt
	float_3 useMoveVec = movementMode == MOVE_DEBUG ? debugMoveVec : moveVec;
	float_3 useDirVec = movementMode == MOVE_DEBUG ? debugDirVec : dirVec;
	mat44<float> normLocal = getCameraSpace(cameras[currentCamera], useMoveVec, useDirVec);
	glm::mat4x4 glmLocal;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			glmLocal[i][j] = normLocal.data[i][j];
		}
	}
	glmLocal = glm::inverse(glmLocal);
	mat44<float> local;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			local.data[i][j] = glmLocal[i][j];
		}
	}


	float_3 cameraPos = useMoveVec + cameras[currentCamera].forAnimate.translate;
	pushConstHDR.numLights = (int)lightPool.size();
	pushConstHDR.camPosX = cameraPos.x;
	pushConstHDR.camPosY = cameraPos.y;
	pushConstHDR.camPosZ = cameraPos.z;
	pushConstHDR.pbrP = 3;
	pushConstantRT.frame = frame;
	pushConstantRT.doReflect = doReflect;
	pushConstantRT.numSamples = numSamples;
	frame++;
	
	for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		mat44<float> test = local*normLocal;
		memcpy(uniformBuffersMappedCamera[frame], &(local),
			sizeof(mat44<float>));
		memcpy(uniformBuffersMappedProj[frame], &(cameras[currentCamera].invPerspective),
			sizeof(mat44<float>));
	}
	
}


void RTSystem::drawFrame() {
	VkResult result;

	uint32_t imageIndex;
	if (renderToWindow) {
		vkWaitForFences(device, 1, inFlightFences.data() + currentFrame, VK_TRUE, UINT64_MAX);

		if (!*activeP) { return; }
		result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS || !*activeP) {
			throw std::runtime_error("ERROR: Unable to acquire a swap chain image in RTSystem.");
		}

		vkResetFences(device, 1, inFlightFences.data() + currentFrame);

	}
	else { //Headless
		if (headlessGuard) return;
		headlessGuard = true;
		imageIndex = currentFrame % MAX_FRAMES_IN_FLIGHT;
	}



	updateUniformBuffers(currentFrame);


	vkResetCommandBuffer(commandBuffers[currentFrame], 0);

	raytrace(commandBuffers[currentFrame]);


	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	transitionImageLayout(rtImages[currentFrame], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
	recordCommandBufferMain(commandBuffers[currentFrame], imageIndex);

	if (renderToWindow) {

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;


		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = commandBuffers.data() + (currentFrame);
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Unable to present a swap chain image in RTSystem.");
		}
	}
	else {
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = commandBuffers.data() + (currentFrame);

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
	}

	currentFrame++; currentFrame %= MAX_FRAMES_IN_FLIGHT;
	//Headless benchmarking
	if (!renderToWindow) {
		headlessFrames++;
		std::cout << "Frames rendered: " << headlessFrames << std::endl;
	}
	transitionImageLayout(rtImages[currentFrame], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
}


void RTSystem::recreateSwapChain() {

	vkDeviceWaitIdle(device);
	handleWindowMinimized(mainWindow);
	if (*activeP)
	{
		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		createFramebuffers();
	}
}

void RTSystem::cleanupSwapChain() {
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
	}
	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}
	vkDestroySwapchainKHR(device, swapChain, nullptr);
}