#pragma once
#include "vulkan/vulkan.h"
#include <optional>
#include "MathHelpers.h"
#include "SceneGraph.h"
#include <algorithm>

enum Platform { PLAT_WIN, PLAT_LIN };
enum  MovementMode { MOVE_STATIC, MOVE_USER, MOVE_DEBUG };

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;



struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
