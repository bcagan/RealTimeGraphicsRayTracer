#pragma once

#ifndef SYS_COMMON
#define SYS_COMMON

#include "vulkan/vulkan.h"
#include <optional>
#include "MathHelpers.h"
#include "SceneGraph.h"
#include <algorithm>





static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}


static bool updateChannelStep(Driver* driver, int ind, SceneGraph* sceneGraphP) {
	if (driver->lastIndex == ind) return false;
	float_3 translate = float_3(0, 0, 0);
	quaternion<float> rotate = quaternion<float>();
	float_3 scale = float_3(1, 1, 1);
	driver->lastIndex = ind;
	switch (driver->channel)
	{
	case CH_TRANSLATE:
		translate = float_3(
			driver->values[ind * 3],
			driver->values[ind * 3 + 1],
			driver->values[ind * 3 + 2]
		);
		sceneGraphP->graphNodes[driver->id].translate = translate;
		break;
	case CH_ROTATE:
		rotate.setAngle(driver->values[ind * 4 + 3]);
		rotate.setAxis(float_3(
			driver->values[ind * 4],
			driver->values[ind * 4 + 1],
			driver->values[ind * 4 + 2]
		));
		sceneGraphP->graphNodes[driver->id].rotation = rotate;
		break;
	default: //SCALE
		scale = float_3(
			driver->values[ind * 3],
			driver->values[ind * 3 + 1],
			driver->values[ind * 3 + 2]
		);
		sceneGraphP->graphNodes[driver->id].scale = scale;
		break;
	}
	return true;
}

static bool updateChannelLinear(Driver* driver, int ind, SceneGraph* sceneGraphP) {
	int nextInd = ind + 1;
	if (nextInd >= driver->times.size()) nextInd = 0;
	float timeInd = driver->times[ind];
	float timeNext = driver->times[nextInd];
	float t = (driver->currentRuntime - timeInd) / (timeNext - timeInd);
	quaternion<float> rotateInd; quaternion<float> rotateNext;
	float_3 translateInd; float_3 translateNext;
	float_3 scaleInd; float_3 scaleNext;
	switch (driver->channel)
	{
	case CH_TRANSLATE:
		translateInd = float_3(
			driver->values[ind * 3],
			driver->values[ind * 3 + 1],
			driver->values[ind * 3 + 2]
		);
		translateNext = float_3(
			driver->values[nextInd * 3],
			driver->values[nextInd * 3 + 1],
			driver->values[nextInd * 3 + 2]
		);
		sceneGraphP->graphNodes[driver->id].translate = translateInd * (1 - t) + translateNext * t;
		break;
	case CH_ROTATE:
		rotateInd = quaternion<float>();
		rotateInd.setAngle(driver->values[ind * 4 + 3]);
		rotateInd.setAxis(float_3(
			driver->values[ind * 4],
			driver->values[ind * 4 + 1],
			driver->values[ind * 4 + 2]
		));
		rotateInd = rotateInd * (1 - t);
		rotateNext = quaternion<float>();
		rotateNext.setAngle(driver->values[nextInd * 4 + 3]);
		rotateNext.setAxis(float_3(
			driver->values[nextInd * 4],
			driver->values[nextInd * 4 + 1],
			driver->values[nextInd * 4 + 2]
		));
		rotateNext = rotateNext * t;
		//This rotation math is correct, so, its a paramater problem
		sceneGraphP->graphNodes[driver->id].rotation = rotateInd + rotateNext;
		break;
	default: //SCALE
		scaleInd = float_3(
			driver->values[ind * 3],
			driver->values[ind * 3 + 1],
			driver->values[ind * 3 + 2]
		);
		scaleNext = float_3(
			driver->values[nextInd * 3],
			driver->values[nextInd * 3 + 1],
			driver->values[nextInd * 3 + 2]
		);
		sceneGraphP->graphNodes[driver->id].scale = scaleInd * (1 - t) + scaleNext * t;
		break;
	}
	return true;
}

static bool updateChannelSlerp(Driver* driver, int ind, SceneGraph* sceneGraphP) {
	int nextInd = ind + 1;
	if (nextInd >= driver->times.size()) nextInd = 0;
	float timeInd = driver->times[ind];
	float timeNext = driver->times[nextInd];
	float t = (driver->currentRuntime - timeInd) / (timeNext - timeInd);
	float omega = 0; float constInd = 0; float constNext = 0; float dotAngle = 0;
	quaternion<float> rotateInd = quaternion<float>();
	quaternion<float> rotateNext = quaternion<float>();
	switch (driver->channel)
	{
	case CH_TRANSLATE:
		return updateChannelLinear(driver, ind, sceneGraphP);
		break;
	case CH_ROTATE:
		rotateInd = quaternion<float>();
		rotateInd.setAngle(driver->values[ind * 4 + 3] * (1 - t));
		rotateInd.setAxis(float_3(
			driver->values[ind * 4],
			driver->values[ind * 4 + 1],
			driver->values[ind * 4 + 2]
		) * (1 - t));
		rotateNext = quaternion<float>();
		rotateNext.setAngle(driver->values[nextInd * 4 + 3] * t);
		rotateNext.setAxis(float_3(
			driver->values[nextInd * 4],
			driver->values[nextInd * 4 + 1],
			driver->values[nextInd * 4 + 2]
		) * t);
		//https://en.wikipedia.org/wiki/Slerp
		dotAngle = rotateInd.normalize().dot(rotateNext.normalize());
		omega = acos(dotAngle);
		//Recalculate vectors without pre-interpolating
		rotateInd.setAngle(driver->values[ind * 4 + 3]);
		rotateInd.setAxis(float_3(
			driver->values[ind * 4],
			driver->values[ind * 4 + 1],
			driver->values[ind * 4 + 2]
		));
		rotateNext = quaternion<float>();
		rotateNext.setAngle(driver->values[nextInd * 4 + 3]);
		rotateNext.setAxis(float_3(
			driver->values[nextInd * 4],
			driver->values[nextInd * 4 + 1],
			driver->values[nextInd * 4 + 2]
		));
		constInd = sin((1 - t) * omega) / (sin(omega));
		constNext = sin(t * omega) / sin(omega);
		sceneGraphP->graphNodes[driver->id].rotation = (rotateInd * constInd) + (rotateNext * constNext);
		break;
	default: //SCALE
		return updateChannelLinear(driver, ind, sceneGraphP);
		break;
	}
	return true;
}

static bool updateTransform(Driver* driver, float frameTime, SceneGraph* sceneGraphP, bool loop = false) {
	float lastTime = driver->currentRuntime;
	if (loop || driver->currentRuntime <= driver->times[driver->times.size() - 1])driver->currentRuntime += frameTime;
	if (loop && driver->currentRuntime > driver->times[driver->times.size() - 1]) {
		driver->currentRuntime -= driver->times[driver->times.size() - 1];
	}
	if (driver->currentRuntime < driver->times[0])
		driver->currentRuntime += driver->times[driver->times.size() - 1];
	float thisTime = driver->currentRuntime;

	int ind = 0;
	for (; ind < driver->times.size(); ind++) {
		if (ind == driver->times.size() - 1 ||
			driver->times[ind + 1] >= thisTime) break;
	}
	switch (driver->interpolation)
	{
	case LINEAR:
		return updateChannelLinear(driver, ind, sceneGraphP);
		break;
	case STEP:
		return updateChannelStep(driver, ind, sceneGraphP);
		break;
	default://SLERP
		return updateChannelSlerp(driver, ind, sceneGraphP);
		break;
	}
}



//Pre-calculate info needed for all points
struct frustumInfo {
	float_3 topNormal;
	float_3 bottomNormal;
	float_3 nearNormal;
	float_3 farNormal;
	float_3 leftNormal;
	float_3 rightNormal;
	float_3 topOrigin;
	float_3 bottomOrigin;
	float_3 nearOrigin;
	float_3 farOrigin;
	float_3 leftOrigin;
	float_3 rightOrigin;
	float nearBottom;
	float nearTop;
	float nearLeft;
	float nearRight;
	float farTop;
	float farLeft;
	float farRight;
	float farBottom;
	float farZ;
	float nearZ;
};

static frustumInfo findFrustumInfo(DrawCamera camera) {
	frustumInfo info;
	//Constraints
	info.farZ = camera.perspectiveInfo.farP;
	info.nearZ = camera.perspectiveInfo.nearP;
	info.nearTop = tan(camera.perspectiveInfo.vfov / 2.0) * info.nearZ;
	info.farTop = tan(camera.perspectiveInfo.vfov / 2.0) * info.farZ;
	info.nearBottom = -tan(camera.perspectiveInfo.vfov / 2.0) * info.nearZ;
	info.farBottom = -tan(camera.perspectiveInfo.vfov / 2.0) * info.farZ;
	info.nearRight = camera.perspectiveInfo.aspect * info.nearTop;
	info.farRight = camera.perspectiveInfo.aspect * info.farTop;
	info.nearLeft = camera.perspectiveInfo.aspect * info.nearBottom;
	info.farLeft = camera.perspectiveInfo.aspect * info.farBottom;
	//Normals
	info.nearNormal = float_3(0, 0, 1);
	info.farNormal = float_3(0, 0, -1);
	info.rightNormal = (
		float_3(info.farRight, info.farTop, info.farZ) -
		float_3(info.nearRight, info.nearTop, info.nearZ)).cross(
			float_3(0, -1, 0)
		).normalize();
	info.leftNormal = (
		float_3(info.farLeft, info.farTop, info.farZ) -
		float_3(info.nearLeft, info.nearTop, info.nearZ)).cross(
			float_3(0, 1, 0)
		).normalize();
	info.topNormal = (
		float_3(info.farRight, info.farTop, info.farZ) -
		float_3(info.nearRight, info.nearTop, info.nearZ)).cross(
			float_3(-1, 0, 0)
		).normalize();
	info.bottomNormal = (
		float_3(info.farRight, info.farBottom, info.farZ) -
		float_3(info.nearRight, info.nearBottom, info.nearZ)).cross(
			float_3(1, 0, 0)
		).normalize();
	//Origins
	info.nearOrigin = float_3(0, 0, info.nearZ);
	info.farOrigin = float_3(0, 0, info.farZ);
	float midZ = info.nearZ + (info.farZ - info.nearZ) / 2.0;
	info.topOrigin = float_3(0, info.nearTop + 0.5 * (info.farTop - info.nearTop), midZ);
	info.bottomOrigin = float_3(0, info.nearBottom + 0.5 * (info.farBottom - info.nearBottom), midZ);
	info.leftOrigin = float_3(info.nearLeft + 0.5 * (info.farLeft - info.nearLeft), 0, midZ);
	info.rightOrigin = float_3(info.nearRight + 0.5 * (info.farRight - info.nearRight), 0, midZ);
	return info;
}

static bool sphereInFrustum(std::pair<float_3, float> boundingSphere, frustumInfo info, mat44<float> toCameraSpace, mat44<float> toWorldSpace) {
	//rotate point into camera space
	float_3 rotatedPoint = toCameraSpace * toWorldSpace * boundingSphere.first;
	//Nan check (case when camera is unintialized)
	if (rotatedPoint.x != rotatedPoint.x || rotatedPoint.y != rotatedPoint.y || rotatedPoint.z != rotatedPoint.z) return false;
	rotatedPoint.z *= -1;

	float_3 fromOrigin;
	float originDist;
	//Project the point on to each plane
	//Near
	fromOrigin = rotatedPoint - info.nearOrigin;
	originDist = info.nearNormal.dot(fromOrigin);
	float_3 nearPoint = rotatedPoint - info.nearNormal * originDist;
	//bool belowNearBound = 

	//Far
	fromOrigin = rotatedPoint - info.farOrigin;
	originDist = info.farNormal.dot(fromOrigin);
	float_3 farPoint = rotatedPoint - info.farNormal * originDist;

	//Right
	fromOrigin = rotatedPoint - info.rightOrigin;
	originDist = info.rightNormal.dot(fromOrigin);
	float_3 rightPoint = rotatedPoint - info.rightNormal * originDist;

	//Left
	fromOrigin = rotatedPoint - info.leftOrigin;
	originDist = info.leftNormal.dot(fromOrigin);
	float_3 leftPoint = rotatedPoint - info.leftNormal * originDist;

	//Top
	fromOrigin = rotatedPoint - info.topOrigin;
	originDist = info.topNormal.dot(fromOrigin);
	float_3 topPoint = rotatedPoint - info.topNormal * originDist;

	//Bottom
	fromOrigin = rotatedPoint - info.bottomOrigin;
	originDist = info.bottomNormal.dot(fromOrigin);
	float_3 bottomPoint = rotatedPoint - info.bottomNormal * originDist;


	//Re-project point into frustum constraints

	//Near
	if (nearPoint.x < info.nearLeft) nearPoint.x = info.nearLeft;
	if (nearPoint.x > info.nearRight) nearPoint.x = info.nearRight;
	if (nearPoint.y < info.nearBottom) nearPoint.y = info.nearBottom;
	if (nearPoint.y > info.nearTop) nearPoint.y = info.nearTop;

	//Far
	if (farPoint.x < info.farLeft) farPoint.x = info.farLeft;
	if (farPoint.x > info.farRight) farPoint.x = info.farRight;
	if (farPoint.y < info.farBottom) farPoint.y = info.farBottom;
	if (farPoint.y > info.farTop) farPoint.y = info.farTop;

	//Right
	if (rightPoint.z > info.farZ) {
		rightPoint.z = info.farZ;
	}
	else if (rightPoint.z < info.nearZ) {
		rightPoint.z = info.nearZ;
	}
	float zPercentage = (rightPoint.z - info.nearZ) / (info.farZ - info.nearZ);
	float rightTop = info.nearTop + (info.farTop - info.nearTop) * zPercentage;
	float rightBottom = info.nearBottom + (info.farBottom - info.nearBottom) * zPercentage;
	if (rightPoint.y > rightTop) {
		rightPoint.y = rightTop;
	}
	else if (rightPoint.y < rightBottom) {
		rightPoint.y = rightBottom;
	}
	fromOrigin = rightPoint - info.rightOrigin;
	originDist = info.rightNormal.dot(fromOrigin);
	rightPoint = rightPoint - info.rightNormal * originDist;

	//Left
	if (leftPoint.z > info.farZ) {
		leftPoint.z = info.farZ;
	}
	else if (leftPoint.z < info.nearZ) {
		leftPoint.z = info.nearZ;
	}
	zPercentage = (leftPoint.z - info.nearZ) / (info.farZ - info.nearZ);
	float leftTop = info.nearTop + (info.farTop - info.nearTop) * zPercentage;
	float leftBottom = info.nearBottom + (info.farBottom - info.nearBottom) * zPercentage;
	if (leftPoint.y > leftTop) {
		leftPoint.y = leftTop;
	}
	else if (leftPoint.y < leftBottom) {
		leftPoint.y = leftBottom;
	}
	fromOrigin = leftPoint - info.leftOrigin;
	originDist = info.leftNormal.dot(fromOrigin);
	leftPoint = leftPoint - info.leftNormal * originDist;

	//Top
	if (topPoint.z > info.farZ) {
		topPoint.z = info.farZ;
	}
	else if (topPoint.z < info.nearZ) {
		topPoint.z = info.nearZ;
	}
	zPercentage = (topPoint.z - info.nearZ) / (info.farZ - info.nearZ);
	float topLeft = info.nearLeft + (info.farLeft - info.nearLeft) * zPercentage;
	float topRight = info.nearRight + (info.farRight - info.nearRight) * zPercentage;
	if (topPoint.x > topRight) {
		topPoint.x = topRight;
	}
	else if (topPoint.x < topLeft) {
		topPoint.x = topLeft;
	}
	fromOrigin = topPoint - info.topOrigin;
	originDist = info.topNormal.dot(fromOrigin);
	topPoint = topPoint - info.topNormal * originDist;

	//Bottom
	if (bottomPoint.z > info.farZ) {
		bottomPoint.z = info.farZ;
	}
	else if (bottomPoint.z < info.nearZ) {
		bottomPoint.z = info.nearZ;
	}
	zPercentage = (bottomPoint.z - info.nearZ) / (info.farZ - info.nearZ);
	float bottomLeft = info.nearLeft + (info.farLeft - info.nearLeft) * zPercentage;
	float bottomRight = info.nearRight + (info.farRight - info.nearRight) * zPercentage;
	if (bottomPoint.x > bottomRight) {
		bottomPoint.x = bottomRight;
	}
	else if (bottomPoint.x < bottomLeft) {
		bottomPoint.x = bottomLeft;
	}
	fromOrigin = bottomPoint - info.bottomOrigin;
	originDist = info.bottomNormal.dot(fromOrigin);
	bottomPoint = bottomPoint - info.bottomNormal * originDist;

	//If ANY re-projected point is within radius, then there is an intersection
	if ((rotatedPoint - nearPoint).norm() <= boundingSphere.second) {
		return true;
	}
	if ((rotatedPoint - farPoint).norm() <= boundingSphere.second) {
		return true;
	}
	if ((rotatedPoint - topPoint).norm() <= boundingSphere.second) {
		return true;
	}
	if ((rotatedPoint - bottomPoint).norm() <= boundingSphere.second) {
		return true;
	}
	if ((rotatedPoint - leftPoint).norm() <= boundingSphere.second) {
		return true;
	}
	if ((rotatedPoint - rightPoint).norm() <= boundingSphere.second) {
		return true;
	}

	//Check if inside frustum
	if (
		rotatedPoint.x < rightPoint.x && rotatedPoint.x > leftPoint.x &&
		rotatedPoint.y < topPoint.y && rotatedPoint.y > bottomPoint.y &&
		rotatedPoint.z > info.nearZ && rotatedPoint.z < info.farZ
		) {
		return true;
	}

	return false;
}



static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {

		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentmodes) {
	for (const VkPresentModeKHR presentMode : availablePresentmodes) {
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, std::pair<int, int> resolution) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}
	else {
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(resolution.first),
			static_cast<uint32_t>(resolution.second)
		};
		actualExtent.width = std::clamp(actualExtent.width,
			capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height,
			capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height);
		return actualExtent;
	}
}


static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties,
	VkPhysicalDevice physicalDevice) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t index = 0; index < memProperties.memoryTypeCount; index++) {
		if ((typeFilter & (1 << index)) && (memProperties.memoryTypes[index].propertyFlags & properties) == properties) {
			return index;
		}
	}
	throw std::runtime_error("ERROR: Unable to find a suitable memory type in VulkanSystem.");
}

#endif // !SYS_COMMON