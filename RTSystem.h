#pragma once
#include "ProgramMode.h"
#include "WindowManager_win.h"
#include "vulkan/vulkan.h"
#include <optional>
#include "MathHelpers.h"
#include "Vertex.h"
#include "SceneGraph.h"
#include "platform.h"
#include "SystemCommonTypes.h"



class RTSystem
{
public:
	RTSystem() {};

	MasterWindow* mainWindow;
	void initVulkan(DrawList drawList, std::string cameraName);
	void cleanup();

	//main loop
	void drawFrame();
	void idle() {
		vkDeviceWaitIdle(device);
	};
	void listPhysicalDevices();
	uint32_t currentFrame = 0;
	uint32_t currentPool = 0;
	Platform platform;
	bool* activeP;
	float_3 moveVec;
	float_3 dirVec;
	float_3 debugMoveVec;
	float_3 debugDirVec;
	MovementMode movementMode;
	std::string deviceName;
	bool playingAnimation = true;
	bool forwardAnimation = true;
	bool useInstancing = false;
	bool useCulling = false;
	int poolSize;

	//Directories
	std::string shaderDir;

	//Headless mode
	bool headlessGuard = true;
	bool renderToWindow = true; //false = headless
	float playbackSpeed = 1;
	void setDriverRuntime(float time);

	//Vertex shader
	std::vector<Vertex> vertices;
	std::vector<std::vector<uint32_t>> indexPoolsMesh;
	std::vector<mat44<float>> transformPoolsMesh;
	std::vector<mat44<float>> transformNormalPoolsMesh;
	std::vector<mat44<float>> transformEnvironmentPoolsMesh;
	std::vector<std::pair<uint32_t, uint32_t>> meshMinMax;
	//Materials and Lights
	std::vector<DrawLight> lightPool;
	std::vector<mat44<float>> worldTolightPool;
	std::vector<mat44<float>> worldTolightPerspPool;
	std::optional<Texture> rawEnvironment;
	Texture LUT;
	std::vector<DrawMaterial> meshMaterials;
	Texture defaultShadowTex;
	//Animation and culling
	std::vector<std::vector<DrawNode>> drawPools;
	std::vector<std::pair<float_3, float>> boundingSpheresMesh;
	std::vector<Driver> nodeDrivers;
	std::vector<Driver> cameraDrivers;
	//Cameras
	std::vector<DrawCamera> cameras;
private:
	//init
	void createInstance(bool verbose = true);
	void setupDebugMessenger();
	void createSurface();
	bool checkValidationSupport();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	int isDeviceSuitable(VkPhysicalDevice device);
	void pickPhysicalDevice();
	void createLogicalDevice();
	int32_t getMemoryType(VkImage image);
	void createAttachments();
	void createSwapChain();
	VkImageView createImageView(VkImage image, VkFormat format,
		VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, int levels = 1);
	void createImageViews();


	struct BuildData {
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
		const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
	};

	struct AS {
		PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
		VkBuffer buf;
		VkDeviceMemory mem;
		VkAccelerationStructureKHR acc;
		void create(VkAccelerationStructureCreateInfoKHR createInfo, VkDevice device);

		VkDeviceAddress address(VkDevice device) {

			VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
			addressInfo.accelerationStructure = acc;
			return vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
		}
	};
	

	void createBLAccelereationStructure(
		std::vector<uint32_t> meshIndicies, 
		std::vector<BuildData>& buildData, 
		VkDeviceAddress scratchAddress,
		VkQueryPool queryPool);
	void compactBLAccelereationStructure(
		std::vector<uint32_t> meshIndicies, 
		std::vector<BuildData> buildData, 
		VkQueryPool queryPool,
		std::vector<AS>& cleanupAS);
	void createBLAccelereationStructures(uint32_t flags);
	void createTLAccelereationStructures();
	void createAccelereationStructures();
	void createDescriptorSetLayout();
	void createGraphicsPipeline(std::string vertShader, std::string fragShader, VkPipeline& pipeline, VkPipelineLayout& layout, int subpass, VkRenderPass inRenderPass);
	void createGraphicsPipelines();
	void createRenderPasses();
	VkShaderModule createShaderModule(const std::vector<char>& shader);
	void createFramebuffers();
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void copyBuffer(VkBuffer source, VkBuffer dest, VkDeviceSize size);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer& buffer,
		VkDeviceMemory& bufferMemory, bool realloc);
	void createVertexBuffer(bool realloc = true);
	void createTransformBuffers(bool realloc = true);
	mat44<float> getCameraSpace(DrawCamera camera, float_3 useMoveVec, float_3 useDirVec);
	void transitionImageLayout(VkImage image, VkFormat format,
		VkImageLayout oldLayout, VkImageLayout newLayout, int layers = 1, int levels = 1);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int level = 0, int face = 0);
	void createIndexBuffers(bool realloc = true, bool andFree = false);
	void generateMipmaps(VkImage image, int32_t x, int32_t y, uint32_t mipLevels, int face = 0);
	void createEnvironmentImage(Texture env, VkImage& image,
		VkDeviceMemory& memory, VkImageView& imageViews, VkSampler& sampler);
	void createTextureImage(Texture tex, VkImage& image,
		VkDeviceMemory& memory, VkImageView& imageView, VkSampler& sampler);
	void createTextureImages();
	void createUniformBuffers(bool realoc = true);
	void createDescriptorPool();
	void createDepthResources();
	void createDescriptorSets();
	void createCommands();
	void recordCommandBufferMain(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void submitFrame(size_t frameIndex, uint32_t imageIndex, bool draw);
	void updateUniformBuffers(uint32_t frame);
	void createImage(uint32_t width, uint32_t height, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags,
		VkMemoryPropertyFlags properties, VkImage& image,
		VkDeviceMemory& imageMemory, int arrayLevels = 1, int levels = 1);

	//main loop
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	void recreateSwapChain();
	void cleanupSwapChain();



	struct PushConst {
		int numLights;
		float camPosX;
		float camPosY;
		float camPosZ;
		float pbrP;
	};
	PushConst pushConstHDR;


	//Vulkan data
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	QueueFamilyIndices familyIndices;
	//Pipeline
	std::vector<VkDeviceMemory> attachmentMemorys;
	std::vector<VkImageView> attachmentImageViews;
	VkPipelineLayout pipelineLayoutRT;
	VkPipelineLayout pipelineLayoutFinal;
	VkPipeline graphicsPipelineRT;
	VkPipeline graphicsPipelineFinal;
	//Rendering
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImage> attachmentImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	//Vertices
	VkBuffer vertexBuffer;
	bool useVertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	std::vector<VkBuffer> meshIndexBuffers;
	std::vector<VkDeviceMemory> meshIndexBufferMemorys;
	std::vector<VkBuffer> meshTransformBuffers;
	std::vector<VkDeviceMemory> meshTransformMemorys;
	//Acceleration Structures

	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
	PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;

	std::vector<AS> blas;
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	//Images
	bool initialFrame = true;
	std::vector<Texture> rawTextures;
	std::vector<Texture> rawCubes;
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemorys;
	std::vector<VkImageView> textureImageViews;
	std::vector<VkSampler> textureSamplers;
	std::vector<VkImage> cubeImages;
	std::vector<VkDeviceMemory> cubeImageMemorys;
	std::vector<VkImageView> cubeImageViews;
	std::vector<VkSampler> cubeSamplers;
	VkImage environmentImage;
	VkDeviceMemory environmentImageMemory;
	VkImageView environmentImageView;
	VkSampler environmentSampler;
	VkImage LUTImage;
	VkDeviceMemory LUTImageMemory;
	VkImageView LUTImageView;
	VkSampler LUTSampler;
	//Uniforms

	std::vector<std::vector<VkBuffer>> uniformBuffersEnvironmentTransformsPools;
	std::vector<std::vector<VkDeviceMemory>> uniformBuffersMemoryEnvironmentTransformsPools;
	std::vector<std::vector<void*>> uniformBuffersMappedEnvironmentTransformsPools;

	std::vector<std::vector<VkBuffer>> uniformBuffersNormalTransformsPools;
	std::vector<std::vector<VkDeviceMemory>> uniformBuffersMemoryNormalTransformsPools;
	std::vector<std::vector<void*>> uniformBuffersMappedNormalTransformsPools;

	std::vector< std::vector<VkBuffer>> uniformBuffersCamerasPools;
	std::vector< std::vector<VkDeviceMemory >> uniformBuffersMemoryCamerasPools;
	std::vector< std::vector<void*>> uniformBuffersMappedCamerasPools;

	std::vector< std::vector<VkBuffer>> uniformBuffersLightsPools;
	std::vector< std::vector<VkDeviceMemory >> uniformBuffersMemoryLightsPools;
	std::vector< std::vector<void*>> uniformBuffersMappedLightsPools;

	std::vector< std::vector<VkBuffer>> uniformBuffersLightTransformsPools;
	std::vector< std::vector<VkDeviceMemory >> uniformBuffersMemoryLightTransformsPools;
	std::vector< std::vector<void*>> uniformBuffersMappedLightTransformsPools;

	std::vector< std::vector<VkBuffer>> uniformBuffersLightPerspectivePools;
	std::vector< std::vector<VkDeviceMemory >> uniformBuffersMemoryLightPerspectivePools;
	std::vector< std::vector<void*>> uniformBuffersMappedLightPerspectivePools;

	std::vector< std::vector<VkBuffer>> uniformBuffersMaterialsPools;
	std::vector< std::vector<VkDeviceMemory >> uniformBuffersMemoryMaterialsPools;
	std::vector< std::vector<void*>> uniformBuffersMappedMaterialsPools;
	VkDescriptorPool descriptorPoolHDR;
	std::vector<VkDescriptorSet> descriptorSetsHDR;
	VkDescriptorPool descriptorPoolFinal;
	std::vector<VkDescriptorSet> descriptorSetsFinal;
	std::vector < VkDescriptorSetLayout> descriptorSetLayouts;

	//Camera
	int currentCamera = 0;

	//Headless debug
	int headlessFrames = 0;

	//RT
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR 
		rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };



	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
	};



	void handleWindowMinimized(MasterWindow* mainWindow)
	{
#ifdef PLATFORM_WIN


		RECT rect;
		GetClientRect(mainWindow->getHwnd(), &rect);
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		while (width <= 0 && height <= 0 && *activeP);
#endif // PLATFORM_WIN
#ifdef PLATFROM_LIN

#endif // PLATFROM_LIN

	}


#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif // NDEBUG
};

