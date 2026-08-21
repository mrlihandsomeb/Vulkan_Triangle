#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <optional>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";
const int MAX_FRAMES_IN_FLIGHT = 2;

//验证层声明
const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
//交换链层声明
const std::vector<const char*> deviceExtentions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif 

VkResult CreatDebugUtilsMessengerEXT(
	VkInstance instance,                           //参数相比createinstance多一个Vkinstaance，因为creatinstance没有父类实例
	const VkDebugUtilsMessengerCreateInfoEXT* pCreatInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreatInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

//物理设备（GPU）队列族的索引：从queueFamilies里面的索引找来的，如果这个families里面的familiy有
//相应功能那么就给这个索引里面的值附上那个索引值i。到时候去哪个families里面找
struct QueueFamilyIndices
{
	//检验是否被真正赋值，因为每一个uint32_t都可能代表一个队列族
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

//交换链具体支持的功能
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;      //内部结构体
	std::vector<VkSurfaceFormatKHR> formats;    //内部结构体
	std::vector<VkPresentModeKHR> presentMode;  /*内部枚举，因为另外一个叫VkSurfacePresentModeKHR的结构体用于把
												  前者和一个具体的窗口表面关联在一起*/
};

//顶点数据
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 uv;

	static VkVertexInputBindingDescription getBindDescription()
	{
		VkVertexInputBindingDescription bindDescription{};
		
		bindDescription.binding = 0;    //顶点数组的索引
		bindDescription.stride = sizeof(Vertex);  //从一个条目到下一个条目的字节数
		bindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		//VK_VERTEX_INPUT_RATE_VERTEX   每个定点后移动到下一个条目
		//VK_VERTEX_INPUT_RATE_INSTANCE 每个实例后移动到下一个条目

		return bindDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescription{};

		attributeDescription[0].binding = 0;
		attributeDescription[0].location = 0;  //glsl的location
		attributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT; //一个坐标属性占32位
		attributeDescription[0].offset = offsetof(Vertex, pos);

		attributeDescription[1].binding = 0;
		attributeDescription[1].location = 1;
		attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescription[1].offset = offsetof(Vertex, color);

		attributeDescription[2].binding = 0;
		attributeDescription[2].location = 2;
		attributeDescription[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescription[2].offset = offsetof(Vertex, uv);

		return attributeDescription;
	}

	bool operator ==(const Vertex& other) const
	{
		return pos == other.pos && color == other.color && uv == other.uv;
	}
};

namespace std 
{
	template<> struct hash<Vertex>
	{
		size_t operator()(const Vertex& vertex) const
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
					(hash<glm::vec3>()(vertex.color) << 1)) << 1) ^
					(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

//统一缓冲对象
struct UniformBufferObject
{
	alignas(16) glm::mat4 model;	//模型变换
	alignas(16) glm::mat4 view;		//视角变换
	alignas(16) glm::mat4 proj;		//透视投影
};

class TriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}


private:

	GLFWwindow* window;									//窗口
	VkInstance instance;								//实例
	VkDebugUtilsMessengerEXT debugMessenger;			//传递信息的
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;	//物理设备（GPU）
	VkDevice device;									//逻辑设备
	VkQueue graphicsQueue;								//跟随逻辑设备一同创建的队列的句柄
	VkQueue presentQueue;								//呈现队列，用于渲染
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;							//交换链
	std::vector<VkImage> swapChainImages;				//这些图像是由交换链的实现创建的，它们将在交换链销毁后自动清理
	std::vector<VkImageView> swapChainImageViews;		//图像视图
	VkFormat swapChainImageFormat;						//初始化详见createSwapChain函数,不保存surfaceFormat是因为后面呢的图像试图和渲染通道只需要format，而不需要colorSpace
	VkExtent2D swapChainImageExtent;					//初始化详见createSwapChain函数
	VkRenderPass renderPass;							//渲染过程
	VkDescriptorSetLayout descriptorSetLayout;			//描述符集布局
	VkDescriptorPool descriptorPool;					//描述符池
	std::vector<VkDescriptorSet> descriptorSets;		//描述符集(为每一个统一缓冲区准备一个)
	VkPipelineLayout pipeLineLayout;					//管线布局，在管线创建前使用
	VkPipeline graphicsPipeLine;
	std::vector<VkFramebuffer> swapChainFramBuffers;    //帧缓冲区
	VkCommandPool commandPool;						    //命令池,用于管理命令缓冲区的内存
	std::vector<VkCommandBuffer> commandBuffers;	    //命令缓冲区
	VkCommandPool stagingCommandPool;					//为填充暂存缓冲区而准备的单独的命令池
	VkBuffer vertexBuffer;								//顶点缓冲区	
	VkDeviceMemory vertexBufferMemory;					//顶点缓冲区分配的内存
	VkBuffer indexBuffer;								//索引缓冲区
	VkDeviceMemory indexBufferMemory;					//索引缓冲区分配的内存
	std::vector<Vertex> vertices;						//顶点
	std::unordered_map<Vertex, uint32_t> uniqueVertices;//无重复顶点
	std::vector<uint32_t> indices;						//顶点索引
	std::vector<VkBuffer> uniformBuffers;				//统一缓冲区
	std::vector<VkDeviceMemory> uniformBuffersMemory;	//统一缓冲区分配的内存
	std::vector<void*> uniformBuffersMapped;			//用于写入并转移到缓冲区的内存
	std::vector<VkSemaphore> imageAvalableSemaphores;   //信号量:image是否可以用
	std::vector<VkSemaphore> renderFinishedSemaphores;  //信号量:image是否画完了
	std::vector<VkFence> inFlightFences;;			    //栅栏帧:是否画完了
	uint32_t currentFrame = 0;
	bool frameBufferResized = false;				    //双重保险，因为有些api在窗口变换大小后并不会返回VK_ERROR_OUT_OF_FATE
	uint32_t mipLevels;									//mipmap级别
	VkImage textureImage;								//纹理图像
	VkDeviceMemory textureImageMemory;					//纹理图像所占内存
	VkImageView textureImageView;						//纹理图像视图
	VkSampler textureSampler;							//纹理采样器
	VkImage depthImage;									//深度图
	VkDeviceMemory depthImageMemory;					//深度图所占内存
	VkImageView depthImageView;							//深度图视图



	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);

	}

	static void frameBufferResizeCallback(GLFWwindow *window,int width,int height)
	{
		auto papp = reinterpret_cast<TriangleApplication*>(glfwGetWindowUserPointer(window));
		papp->frameBufferResized = true;
	}

	void initVulkan()
	{
		creatInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeLine();
		createCommandPool();
		createDepthResources();
		createFrameBuffers();
		createTextureImage();
		createTextureImageView();
		createTetureSampler();
		loadModel();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObiects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device); //等待device内部任务完成后结束
	}

	void cleanup()
	{
		destroySyncObjects();           //封装了多个vkDestroySemaphore()和vkDestroyFence()
		vkDestroyCommandPool(device, stagingCommandPool, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		destroyUniformBuffers();		//封装了多个vkDestroyBuffer和vkFreeMemory()
		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexBufferMemory, nullptr);
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);
		cleanUpDepthResources();		//封装了多个vkDestroyImageView(),vkFreeMemory(),vkDestroyImage()
		destroyFrameBuffers();          //封装了多个vkDestroyFrameBuffer()来摧毁帧缓冲
		vkDestroyPipeline(device, graphicsPipeLine, nullptr);
		vkDestroyPipelineLayout(device, pipeLineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		destroyImageViews();			//封装了多个vkDestroyImageView()来摧毁多个视图
		vkDestroySwapchainKHR(device, swapChain, nullptr);
		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);
		vkDestroyImage(device, textureImage, nullptr);
		vkDestroyDevice(device, nullptr);
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);


		glfwDestroyWindow(window);

		glfwTerminate();           //中止glfw的库，所有glfw有关的不能再次使用，除非使用glfwinit()
	}

	void creatInstance()
	{
		//验证层验证
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layer requested,but not available");
		}

		VkApplicationInfo appinfo{};
		appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appinfo.pApplicationName = "Triangle";
		appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appinfo.pEngineName = "No Engine";
		appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appinfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo creatInfo{};
		creatInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		creatInfo.pApplicationInfo = &appinfo;

		auto extentions = getRequiredExtentions();
		creatInfo.enabledExtensionCount = static_cast<uint32_t>(extentions.size());
		creatInfo.ppEnabledExtensionNames = extentions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreatInfo{}; //放在if外来确保不会在创建前被销毁,原因详见下面的pnext，是指针
		if (enableValidationLayers)
		{
			creatInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			creatInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreatInfo);
			creatInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreatInfo;
		}
		else
		{
			creatInfo.enabledLayerCount = 0;
			creatInfo.pNext = nullptr;
		}
		if (vkCreateInstance(&creatInfo, nullptr, &instance) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to creat instance");
		}

		/////////显示vulkan的现有extentions，但不会启动，需要手动开启
		/////////前面的关于glfw的extention开启了，因为是必要的
		///////大括号不能删，因为与上面变量重名

		{
			uint32_t extentionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extentionCount, nullptr);
			std::vector<VkExtensionProperties> extentions(extentionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extentionCount, extentions.data());
			std::cout << "available extensions:\n";

			for (const auto& extension : extentions) {
				std::cout << '\t' << extension.extensionName << '\n';
			}
		}

		////////
	}

	//debug验证层 
	void setupDebugMessenger()
	{
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT creatInfo{};
		populateDebugMessengerCreateInfo(creatInfo);

		if (CreatDebugUtilsMessengerEXT(instance, &creatInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to setup debug messenger");
		}

	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& creatInfo)
	{
		creatInfo = {};
		creatInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		creatInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		creatInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		creatInfo.pfnUserCallback = debugCallBack;
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (!layerFound)
			{
				return false;
			}
		}
		return true;
	}

	std::vector<const char*> getRequiredExtentions()
	{
		uint32_t glfwExtentionCount = 0;
		const char** glfwExtentions;
		glfwExtentions = glfwGetRequiredInstanceExtensions(&glfwExtentionCount);
		std::vector<const char*> extentions(glfwExtentions, glfwExtentions + glfwExtentionCount);
		//如果启用验证层
		if (enableValidationLayers)
		{
			extentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			//这是个宏，宏的内容就是文档里给出的那个名字 VK_EXT_debug_util
		}

		return extentions;
	}

	//具体这个debugMessenger会干什么
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pcallBackData,
		void* pUserData)
		//带bit的是枚举类型，不带的是普通的结构体，flag也是一种类型，第三个变量还有一个类似名字的变量
		//带flag，它类似于从位上进行操作的一系列值。
	{
		std::cerr << "Validation layer:" << pcallBackData->pMessage <<'\n'<< std::endl;

		return VK_FALSE;
	}

	//物理设备查照
	void pickPhysicalDevice()
	{
		uint32_t deviceCount{ 0 };
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		//验证设备是否可用
		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("fail to find a suitable GPU");
		}

	}

	bool isDeviceSuitable(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		QueueFamilyIndices indice = findQueueFamilies(physicalDevice);

		//验证交换链是否开启
		bool extentionSupported = checkDeviceSupported(physicalDevice);

		//验证交换链是否功能齐全
		bool swapChainAdequate = false;
		if (extentionSupported)
		{
			SwapChainSupportDetails details = querySwapChainSupport(physicalDevice);
			swapChainAdequate = !details.formats.empty() && !details.presentMode.empty();
		}

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			deviceFeatures.geometryShader &&
			indice.isComplete() &&
			extentionSupported &&
			deviceFeatures.samplerAnisotropy &&
			swapChainAdequate;
	}

	bool checkDeviceSupported(VkPhysicalDevice physicalDevice)
	{
		uint32_t extentionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extentionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtentions(extentionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extentionCount, availableExtentions.data());

		std::set<std::string> requiredExtentions(deviceExtentions.begin(), deviceExtentions.end());

		for (const auto& extention : availableExtentions)
		{
			requiredExtentions.erase(extention.extensionName);
		}
		return requiredExtentions.empty();
	}

	//物理设备队列族查照
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i{ 0 };
		for (auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 isPresentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &isPresentSupport);

			if (isPresentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}
			i++;
		}

		return indices;
	}

	//逻辑设备创建
	void createLogicalDevice()
	{
		QueueFamilyIndices indice = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies{ indice.graphicsFamily.value(),indice.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreatinfo{};
			queueCreatinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreatinfo.queueFamilyIndex = queueFamily;
			queueCreatinfo.queueCount = 1;
			queueCreatinfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreatinfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(uniqueQueueFamilies.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtentions.size());
		createInfo.ppEnabledExtensionNames = deviceExtentions.data();

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create logical device");
		}

		//第二个参数是索引（队列族索引），第三个参数也是索引（不过这个索引的创建的队列的索引，因为之创建了一个所以为0）
		vkGetDeviceQueue(device, indice.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indice.presentFamily.value(), 0, &presentQueue);
	}

	//创建surface，使用glfw集成的
	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create surface");
		}
	}

	//填充交换链信息的函数
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

		uint32_t formatCount{ 0 };
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
		}

		uint32_t presentCount{ 0 };
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
		if (presentCount != 0)
		{
			details.presentMode.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, details.presentMode.data());
		}


		return details;
	}

	//寻找合适的交换链内部组合(三个)
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& format : availableFormats)
		{
			//与材质的格式不同是因为windows原生的呈现方式就是B8G8R8A8
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return format;
			}
		}

		return availableFormats[0];   //没招了选的最坏的
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto& presentMode : availablePresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return presentMode;
			}
		}

		//return VK_PRESENT_MODE_IMMEDIATE_KHR;      //立即呈现
		return VK_PRESENT_MODE_FIFO_KHR;		   //应用渲染出来的图像放入队列，如果屏幕刷新，则呈现最新的，如果队列满了，会堵塞程序
		//return VK_PRESENT_MODE_FIFO_RELAXED_KHR;   //和第二个有点不同，当队列为空时，如果图像补充，不会等待屏幕的刷新率而是直接呈现
		//return VK_PRESENT_MODE_MAILBOX_KHR;		   //和第二个有所不同，当队列满的时候，不会阻塞应用程序，而是直接把最前面的顶掉，又称为三缓冲

		/*对于三缓冲来说，因为他从不阻塞程序，会导致gpu一直工作，没有休息时间
		  而对于FIFO那两个来说虽然没前者能耗高，但是他们延迟会高于三缓冲*/
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t> (width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			//此处的 clamp 函数用于将 width 和 height 的值限制在实现支持的允许的最小和最大范围之间。
			return actualExtent;
		}
	}

	//创建交换链
	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentMode);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		//有些硬件需要最小的画图数量，比如双缓冲最起码要俩图像
		//+1是为了不让cpu因为没有空闲的图像而停滞
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
			//防止溢出
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.presentMode = presentMode;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; //指定每个图像包含的层数。 除非正在开发立体 3D 应用程序，否则此值始终为 1
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		/*所用的宏是直接绘画，或者还有先将图像渲染到单独的图像中，以执行诸如后处理之类的操作VK_IMAGE_USAGE_TRANSFER_DST_BIT*/

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

		//在大多数硬件上，这两个索引可能相同（例如独立显卡同时支持图形和呈现），但也可能不同（例如某些笔记本：独立显卡渲染，集成显卡负责显示）。
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
		/*Vulkan 提供了两种共享模式：
		独占模式 (VK_SHARING_MODE_EXCLUSIVE)：图像一次只被一个队列族拥有。当需要切换所有者时，必须显式进行所有权转移（通常通过队列同步操作）。这是性能最高的模式
		并发模式 (VK_SHARING_MODE_CONCURRENT)：图像可以被多个队列族同时访问，无需所有权转移。但需要提前指定所有可能访问的队列族索引，驱动会做同步处理。*/
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		//VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 完全不透明
		//VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR  预乘 Alpha
		//VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR 非预乘 Alpha。混合时会由窗口管理器进行“源RGB值 × 源Alpha值”的计算
		//VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR 继承系统设置。Alpha 混合行为完全由窗口管理器决定。这个模式比较特殊，它把混合方式的控制权交给了外部，这意味着行为可能会变得不可预测，不推荐在常规渲染中使用。
		createInfo.clipped = VK_TRUE; //如果为true，则不渲染被遮挡的东西性能好，除非你想要根据被遮挡的东西预测什么
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create a swap chain");
		}

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainImageExtent = extent;

	}

	//重新创建交换链,图像视图,帧缓冲
	void recreateSwapChain()
	{
		int width{ 0 }, height{ 0 };
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		//保留旧的交换链句柄，方便后续销毁
		VkSwapchainKHR oldSwapChain = swapChain;

		vkDeviceWaitIdle(device);

		cleanUpDepthResources();
		destroyFrameBuffers();
		swapChainFramBuffers.clear();
		destroyImageViews();
		swapChainImageViews.clear();

		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentMode);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.clipped = VK_TRUE;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.imageArrayLayers = 1;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.minImageCount = imageCount;
		createInfo.oldSwapchain = oldSwapChain;
		createInfo.presentMode = presentMode;
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.surface = surface;
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to recreate swapchain");
		}

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainImageExtent = extent;

		createDepthResources();
		createImageViews();
		createFrameBuffers();

		vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
	}

	//创建图像视图
	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT ,1);
		}
	}

	//封装一个销毁函数
	void destroyImageViews()
	{
		for (auto view : swapChainImageViews)
		{
			vkDestroyImageView(device, view, nullptr);
		}
	}

	//描述符：一种联系显卡与cpu端代码资源的类型，用于联通各种缓冲区与glsl代码内部的binding所定义的需要的内存
	//创建描述符集布局
	void createDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = 1;		   //对应GLSL里面的ubo对象的大小（几个）
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; //仅和图像采样的描述符相关

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	//采样图像与采样器的结合产物
		samplerLayoutBinding.pImmutableSamplers = nullptr;	//不可变采样器,词条只有在type为VK_DESCRIPTOR_TYPE_SAMPLER或者VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER才生效
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2>bindings{ uboLayoutBinding,samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create descriptor set layout");
		}

	}

	//创建描述符池
	void createDescriptorPool()
	{
		std::array<VkDescriptorPoolSize,2> size{};
		size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		size[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		size[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = static_cast<uint32_t>(size.size());
		createInfo.pPoolSizes = size.data();
		createInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create descriptor pool");
		}
	}

	//创建并填写描述符集(会随着描述符池的销毁一并销毁)
	void createDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate descriptor sets");
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet,2> writeDescriptor{};
			writeDescriptor[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor[0].descriptorCount = 1;
			writeDescriptor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptor[0].dstBinding = 0;
			writeDescriptor[0].dstSet = descriptorSets[i];
			writeDescriptor[0].dstArrayElement = 0;		//当前写入的描述符对应描述符集中的哪一个，进而对应glsl相对应的第几个元素如ubo[0]
			writeDescriptor[0].pBufferInfo = &bufferInfo;
			writeDescriptor[0].pImageInfo = nullptr;
			writeDescriptor[0].pTexelBufferView = nullptr;
			writeDescriptor[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor[1].descriptorCount = 1;
			writeDescriptor[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptor[1].dstArrayElement = 0;
			writeDescriptor[1].dstBinding = 1;
			writeDescriptor[1].dstSet = descriptorSets[i];
			writeDescriptor[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptor.size()), writeDescriptor.data(), 0, nullptr);

		}
	}

	//创建渲染通道
	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		/*
		* VK_ATTACHMENT_LOAD_OP_LOAD：保留附件的现有内容
		* VK_ATTACHMENT_LOAD_OP_CLEAR：在开始时将值清除为常量
		* VK_ATTACHMENT_LOAD_OP_DONT_CARE：现有内容未定义；我们不在乎它们
		*/
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		/*
		* VK_ATTACHMENT_STORE_OP_STORE：渲染的内容将存储在内存中，并且可以稍后读取
		* VK_ATTACHMENT_STORE_OP_DONT_CARE：渲染操作后帧缓冲区的内容将未定义
		*/
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; //给模板用的，跟颜色缓冲区附件没有关系
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		/*
		* VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL：用作颜色附件的图像
		* VK_IMAGE_LAYOUT_PRESENT_SRC_KHR：在交换链中呈现的图像
		* VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL：用作内存复制操作目标的图像
		*/

		VkAttachmentReference colorAttachmentRefference{};
		colorAttachmentRefference.attachment = 0;
		colorAttachmentRefference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference{};
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachmentReference.attachment = 1;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRefference;
		subpass.pDepthStencilAttachment = &depthAttachmentReference;
		/*
		* pInputAttachments：从着色器读取的附件
		* pResolveAttachments：用于多重采样颜色附件的附件
		* pDepthStencilAttachment：用于深度和模板数据的附件
		* pPreserveAttachments：此子过程未使用但必须保留数据的附件
		*/

		VkSubpassDependency dependency{};    //再gpu执行子过程中的开头保安和过程中的等待保安
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;   //等待图像管线颜色输出阶段和深度写入完成完成后，再开始子渲染过程
		dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;  //前一个阶段一定要完成什么，0代表啥也不干
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		/*
		* stage如果为多个阶段，满足一个即可(满足一个就开始等)
		* acess如果为多个阶段，全部满足才行
		*/

		std::array<VkAttachmentDescription, 2> attachments{ colorAttachment,depthAttachment };

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassCreateInfo.pAttachments = attachments.data();
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create render pass");
		}
	}

	//创建图像管线
	/*
	* 顶点输入阶段
	* 顶点着色器
	* 表面细分着色器（可选）
	* 几何着色器（可选）
	* 光栅化阶段
	* 片段着色器阶段
	* 颜色混合阶段呢
	*/
	void createGraphicsPipeLine()
	{
		//着色器阶段
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };


		//顶点输入阶段
		auto bindDescription = Vertex::getBindDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo inputCreateInfo{};
		inputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		inputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		inputCreateInfo.vertexBindingDescriptionCount = 1;
		inputCreateInfo.pVertexBindingDescriptions = &bindDescription;


		//输入汇编（位于顶点输入之后，顶点着色器之前）
		VkPipelineInputAssemblyStateCreateInfo assemblyCreateInfo{};
		assemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		/*
		* `VK_PRIMITIVE_TOPOLOGY_POINT_LIST`：来自顶点的点
		* `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`：每 2 个顶点之间的直线，不复用
		* `VK_PRIMITIVE_TOPOLOGY_LINE_STRIP`：每条线的结束顶点用作下一条线的起始顶点
		* `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`：每 3 个顶点组成的三角形，不复用
		* `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP`：每个三角形的第二个和第三个顶点用作下一个三角形的前两个顶点
		*/
		assemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
		//使用可提高顶点复用性


		//光栅化
		VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo{};
		rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCreateInfo.depthClampEnable = VK_FALSE; //如果非默认（true）情况下会把深度值超出范围的强行放在最近的边界内,使用需开启gpu功能
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE; //设置为 VK_TRUE，则几何体永远不会通过光栅化器阶段。
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		/*
		* VK_POLYGON_MODE_FILL：用片段填充多边形的区域
		* VK_POLYGON_MODE_LINE：多边形边缘绘制为线条
		* VK_POLYGON_MODE_POINT：多边形顶点绘制为点
		*/
		rasterizationCreateInfo.lineWidth = 1.0f; //粗度大于1.0f的需要开启gpu的widelines功能。
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT; //剔除的面
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationCreateInfo.depthBiasClamp = 0.0f;
		rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;


		//多重采样
		VkPipelineMultisampleStateCreateInfo multiSamepleCreateInfo{};
		multiSamepleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multiSamepleCreateInfo.sampleShadingEnable = VK_FALSE;
		multiSamepleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multiSamepleCreateInfo.minSampleShading = 1.0f;
		multiSamepleCreateInfo.pSampleMask = nullptr;
		multiSamepleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multiSamepleCreateInfo.alphaToOneEnable = VK_FALSE;


		//深度和模板测试
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
		depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilInfo.depthTestEnable = VK_TRUE;	//是否应将新片段的深度与深度缓冲区进行比较，以查看是否应丢弃它们
		depthStencilInfo.depthWriteEnable = VK_TRUE;//是否应将通过深度测试的片段的新深度实际写入深度缓冲区
		depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilInfo.minDepthBounds = 0.0f;
		depthStencilInfo.maxDepthBounds = 1.0f;
		depthStencilInfo.stencilTestEnable = VK_FALSE;
		depthStencilInfo.front = {};
		depthStencilInfo.back = {};


		//颜色混合阶段
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
		colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCreateInfo.logicOpEnable = VK_FALSE;		//true后自动禁用attachment里面的颜色混合
		colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendCreateInfo.attachmentCount = 1;
		colorBlendCreateInfo.pAttachments = &colorBlendAttachment;  //使用哪个取决于shader.frag里面的layout（location=？）
																	//为什么片段着色器的东西会来到颜色混合阶段？
																	//因为从这个阶段开始，走出计算的流水线了，开始向特定的显存（帧缓冲区）写入东西
		colorBlendCreateInfo.blendConstants[0] = 0.0f;		//如果上面的attacment里面的factor里的宏有constant，那么就使用这个
		colorBlendCreateInfo.blendConstants[1] = 0.0f;
		colorBlendCreateInfo.blendConstants[2] = 0.0f;
		colorBlendCreateInfo.blendConstants[3] = 0.0f;


		//视口和裁剪矩形
		VkViewport viewPort{};
		viewPort.x = 0.0f;
		viewPort.y = 0.0f;
		viewPort.width = (float)swapChainImageExtent.width;
		viewPort.height = (float)swapChainImageExtent.height;
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;
		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = swapChainImageExtent;

		VkPipelineViewportStateCreateInfo viewPortCreateInfo{};
		viewPortCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewPortCreateInfo.viewportCount = 1;
		viewPortCreateInfo.scissorCount = 1;
		viewPortCreateInfo.pViewports = &viewPort; //如果不设置，则将要在绘制阶段动态设计，无性能损耗，需开启gpu功能,在创建逻辑设备时就已经设置好了。
		viewPortCreateInfo.pScissors = &scissor;   //如果不设置，则将要在绘制阶段动态设计，无性能损耗，需开启gpu功能,在创建逻辑设备时就已经设置好了。


		//可变状态
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicCreateInfo{};
		dynamicCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicCreateInfo.pDynamicStates = dynamicStates.data();


		//管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipeLineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create pipeline layout");
		}

		//创建
		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.stageCount = 2;
		createInfo.pStages = shaderStages;
		createInfo.pVertexInputState = &inputCreateInfo;
		createInfo.pInputAssemblyState = &assemblyCreateInfo;
		createInfo.pViewportState = &viewPortCreateInfo;
		createInfo.pMultisampleState = &multiSamepleCreateInfo;
		createInfo.pRasterizationState = &rasterizationCreateInfo;
		createInfo.pDepthStencilState = &depthStencilInfo;
		createInfo.pColorBlendState = &colorBlendCreateInfo;
		createInfo.pDynamicState = &dynamicCreateInfo;

		createInfo.layout = pipeLineLayout;
		createInfo.renderPass = renderPass;
		createInfo.subpass = 0; //渲染过程的索引，代表这个图像管线用于renderpass的哪一个子过程
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &graphicsPipeLine) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create graphics pipeline");
		}



		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	//读取着色器spir-v
	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("fail to open the file");
		}

		size_t fileSize = file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	//创建着色器模块
	VkShaderModule createShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //SPIR-V 本质上是一串 32 位（4 字节）的机器指令，不是字节流。

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create shaderModule");
		}

		return shaderModule;
	}

	//加载模型
	void loadModel()
	{
		tinyobj::attrib_t attrib;					//顶点坐标，法向坐标，uv坐标
		std::vector<tinyobj::shape_t> shapes;		//索引，因为可能有多个物体，所以有多个索引
		std::vector<tinyobj::material_t> materials;	//顶点颜色属性，材质路径
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};
				vertex.color = { 1.0f,1.0f,1.0f };	//颜色取自材质贴图，故没有颜色
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f-attrib.texcoords[2 * index.texcoord_index + 1]
				};
				//obj格式默认坐标0点在下方，而vulkan0点在上方。所以需要把uv坐标的垂直轴上下翻转，1.0f是因为

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				
				indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	//创建顶点缓冲区
	void createVertexBuffer()
	{
		VkDeviceSize size = sizeof(Vertex) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		createBuffer(size, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			stagingBuffer, stagingBufferMemory);
		//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 这个东西对于cpu可见，用于cpu向内存储顶点信息
		//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT cpu对于这块内存的更新是实时的,用于频繁需要cpu更新的内存,如果没有，需要刷新
		/*本质：顶点数据由cpu内存掌管，gpu隐射只是去看这片内存，没有COHERENT, 数据可能在三级缓存内，没有及时更新到RAM上
		 *需要在cpu调用vkFlushMappedMemoryRanges来把数据放到RAM上，如果有COHERENT，数据就可以直接放在RAM上
		 */
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, vertices.data(), size);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBuffer, vertexBufferMemory);

		copyBuffer(stagingBuffer, vertexBuffer, size);

		vkFreeMemory(device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	//创建索引缓冲区
	void createIndexBuffer()
	{
		VkDeviceSize size = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, indices.data(), (size_t)size);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, size);
		
		vkFreeMemory(device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	//创建统一缓冲区
	void createUniformBuffers() 
	{
		VkDeviceSize size = sizeof(UniformBufferObject);

		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				uniformBuffers[i], uniformBuffersMemory[i]);
			vkMapMemory(device, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
		}
	}

	//封装一个销毁函数
	void destroyUniformBuffers()
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		}
	}

	//创建缓冲区
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = usage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &createInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create buffer");
		}

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};

		allocInfo.allocationSize = memRequirements.size;
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate memort");
		}

		vkBindBufferMemory(device, buffer, bufferMemory, 0);

	}

	//拷贝缓冲区
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer stagingCommandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(stagingCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(stagingCommandBuffer);
	}

	//寻找合适的内存类型
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("fail to find suitable memory type");
	}

	//创建帧缓冲
	void createFrameBuffers()
	{
		swapChainFramBuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainFramBuffers.size(); ++i)
		{
			std::array<VkImageView, 2> attachments{ swapChainImageViews[i],depthImageView };

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.width = swapChainImageExtent.width;
			createInfo.height = swapChainImageExtent.height;
			createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfo.pAttachments = attachments.data();
			createInfo.layers = 1;						//该帧缓冲中每个附件图像的数组层数（即图像包含多少独立的 2D 层）

			if (vkCreateFramebuffer(device, &createInfo, nullptr, &swapChainFramBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("fail to create a framebuffer");
			}
		}
	}

	//封装一个销毁函数
	void destroyFrameBuffers()
	{
		for (auto frameBuffer : swapChainFramBuffers)
		{
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
		}
	}

	//创建命令池
	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		//VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 提示命令缓冲区经常被新的命令重新记录
		//VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 允许单独重新记录命令缓冲区，如果没有此标志，则必须一起重置所有命令缓冲区
		createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); //指定命令池分配的缓冲区是给什么队列用的

		if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create command pool");
		}

		VkCommandPoolCreateInfo stagingPoolCreateInfo{};

		stagingPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		stagingPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		stagingPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &stagingPoolCreateInfo, nullptr, &stagingCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create staging command pool");
		}
	}

	//创建命令缓冲区
	void createCommandBuffers()
	{
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};

		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		//VK_COMMAND_BUFFER_LEVEL_PRIMARY 可以提交到队列执行，但不能在其他缓冲区调用
		//VK_COMMAND_BUFFER_LEVEL_SECONDARY 不能直接提交到队列，需要在主命令缓冲区调用,可用于gpu多线程
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate command buffer");
		}
	}

	//记录命令缓冲区的命令,并开启渲染通道
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		//VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 命令缓冲区将在执行(gpu执行缓冲区中的内容)一次后立即重新记录。
		//VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT 这是一个辅助命令缓冲区，它将完全位于一个渲染通道内。也就是说他必须在一个已经活动的渲染通道内调用
		//VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT 命令缓冲区在已处于挂起执行(提交但未完全执行)状态时可以重新提交。
		beginInfo.pInheritanceInfo = nullptr; //用于辅助命令缓冲区,决定继承主命令缓冲区什么状态

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to begin recording command buffer");
		}
		//如果命令缓冲区已经被记录过一次，那么对 vkBeginCommandBuffer 的调用将隐式重置它。

		VkRenderPassBeginInfo renderPassBeginInfo{};

		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapChainFramBuffers[imageIndex];
		renderPassBeginInfo.renderArea.extent = swapChainImageExtent;
		renderPassBeginInfo.renderArea.offset = { 0,0 };
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f,0.0f,0.0f,1.0f} };
		clearValues[1].depthStencil = { 1.0f,0 };
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		//VK_SUBPASS_CONTENTS_INLINE 渲染通道命令将嵌入到主命令缓冲区本身中，并且不会执行辅助命令缓冲区。
		//VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS 渲染通道命令将从辅助命令缓冲区执行。

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeLine);

		VkViewport viewPort{};
		viewPort.x = 0.0f;
		viewPort.y = 0.0f;
		viewPort.width = static_cast<float>(swapChainImageExtent.width);
		viewPort.height = static_cast<float>(swapChainImageExtent.height);
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewPort);

		VkRect2D scissor{};
		scissor.extent = swapChainImageExtent;
		scissor.offset = { 0,0 };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeLineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to record command buffer");
		}
	}

	//绘画缓冲区
	void drawFrame()
	{
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex{};
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvalableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("fail to acquire next image");
		}
		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
		updateUniformBuffer(currentFrame);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphore[] = { imageAvalableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphore;
		submitInfo.pWaitDstStageMask = waitStages;
		VkSemaphore signaledSemaphore[] = { renderFinishedSemaphores[imageIndex]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signaledSemaphore;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to submit command buffer");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signaledSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR||frameBufferResized)
		{
			frameBufferResized = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("fail to present queue");
		}


		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	//更新统一缓冲区
	void updateUniformBuffer(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainImageExtent.width / (float)swapChainImageExtent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;
		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	//创建同步对象
	void createSyncObiects()
	{
		imageAvalableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(swapChainFramBuffers.size());

		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;		//创建一个有信号的fence用来激发第一帧

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvalableSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("fail to create sync objects");
			}
		}

		for (size_t i = 0; i < swapChainFramBuffers.size(); ++i)
		{
			if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("fail to create sync objects");
			}
		}
	}

	//封装一个摧毁函数
	void destroySyncObjects()
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(device, imageAvalableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		for (size_t i = 0; i < swapChainFramBuffers.size(); ++i)
		{
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		}
	}

	//创建纹理图像并分配内存
	void createTextureImage()
	{
		int texWidth{}, texHeight{}, texChannels{};
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
		VkDeviceSize size = texWidth * texHeight * 4;	//jpg图像一个像素的大小是4字节，每个通道为usinged char,大小为1字节

		if (!pixels)
		{
			throw std::runtime_error("fail to load texture image");
		}

		VkBuffer stagingBuffer{};
		VkDeviceMemory stagingBufferMemory{};
		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(size));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);

		createImage2D(texWidth, texHeight,mipLevels,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			textureImage, textureImageMemory);

		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

		vkFreeMemory(device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	//创建纹理图像视图
	void createTextureImageView()
	{
		textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	}

	//创建纹理采样器
	void createTetureSampler()
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		createInfo.magFilter = VK_FILTER_LINEAR;	//几何体片段大于纹理图像，发生过采样，会产生马赛克，需要和周围像素采样求均值
		createInfo.minFilter = VK_FILTER_LINEAR;	//几何体片段少于纹理图像，发生欠采样，会导致频繁变化的材质会变的模糊（如果以锐角观测几何体则更为严重）
		//VK_FILTER_LINEAR 选取周围4个纹素进行平均
		//VK_FILTER_NEAREST 选最近的对应纹素进行写入
		createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//UVW对应XYZ，这是寻址模式应该干什么（当纹理坐标不再处于0到1之间，如纹理为0到2，这个时候可以在一个集合体上画四个纹理）
		//VK_SAMPLER_ADDRESS_MODE_REPEAT：在超出图像尺寸时重复纹理。
		//VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT：类似于重复，但在超出尺寸时反转坐标以镜像图像。
		//VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE：获取最接近超出图像尺寸的坐标的边缘颜色。
		//VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE：类似于夹紧到边缘，但改为使用与最近边缘相反的边缘。
		//VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER：在采样超出图像尺寸时返回纯色。
		createInfo.anisotropyEnable = VK_TRUE;	//各向异性过滤
		createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;			//最大采样数
		createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		createInfo.compareEnable = VK_FALSE;	//会和一个值作比较，结果用于过滤
		createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		createInfo.unnormalizedCoordinates = VK_FALSE;	
		//如果为true，则会使用(0,texWidth)(0,texHeight)当作纹理坐标轴的寻址
		//如果为false，则会使用(0,1)作为每个轴的寻址范围
		createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;	//lod相当与mipmap层
		createInfo.mipLodBias = 0.0f;
		createInfo.maxLod = VK_LOD_CLAMP_NONE;
		createInfo.minLod = 0.0f;

		if (vkCreateSampler(device, &createInfo, nullptr, &textureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create texture sampler");
		}

	}

	//生成mipmaps
	void generateMipmaps(VkImage image,VkFormat format,int32_t width,int32_t height,uint32_t mipLevels)
	{
		VkFormatProperties properties{};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
		if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		{
			throw std::runtime_error("textture image does not support linear bit");
		}

		VkCommandBuffer stagingCommandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barier{};
		barier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barier.image = image;
		barier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barier.subresourceRange.layerCount = 1;
		barier.subresourceRange.baseArrayLayer = 0;
		barier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;
		for (uint32_t i = 1; i < mipLevels; ++i)
		{
			barier.subresourceRange.baseMipLevel = i - 1;
			barier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			vkCmdPipelineBarrier(stagingCommandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0,0,0 };
			blit.srcOffsets[1] = { mipWidth,mipHeight,1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.mipLevel = i - 1;
			blit.dstOffsets[0] = { 0,0,0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1,mipHeight > 1 ? mipHeight / 2 : 1,1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.layerCount = 1;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.mipLevel = i;

			vkCmdBlitImage(stagingCommandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);
			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barier.subresourceRange.baseMipLevel = mipLevels - 1;
		barier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barier.dstAccessMask = VK_ACCESS_NONE;
		vkCmdPipelineBarrier(stagingCommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barier);
		barier.subresourceRange.levelCount = mipLevels;
		barier.subresourceRange.baseMipLevel = 0;
		barier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barier.srcAccessMask = VK_ACCESS_NONE;
		barier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(stagingCommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barier);

		endSingleTimeCommands(stagingCommandBuffer);
	}

	//创建图像
	void createImage2D(uint32_t width, uint32_t height,uint32_t mipLevels,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createInfo.arrayLayers = 1;
		createInfo.extent.depth = 1;
		createInfo.extent.width = width;
		createInfo.extent.height = height;
		createInfo.format = format;
		createInfo.imageType = VK_IMAGE_TYPE_2D;
		createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//VK_IMAGE_LAYOUT_UNDEFINED 不关心，随便什么样都行，最快
		//VK_IMAGE_LAYOUT_PREINITIALIZED 告诉驱动这个图象原来有用户的数据，要保留完整的内容
		createInfo.mipLevels = mipLevels;
		createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.tiling = tiling;
		//VK_IMAGE_TILING_LINEAR：纹素按行主序排列，就像我们的 pixels 数组一样
		//VK_IMAGE_TILING_OPTIMAL：纹素以实现定义的顺序排列，以实现最佳访问
		createInfo.usage = usage;
		createInfo.flags = 0;
		//和稀疏图像相关的图像具有一些可选的flags
		//稀疏图像：并非所有内存都有实际的像素的图像，例如为体素地形使用3d纹理，部分flags可以避免存储大量的空气值
		//VK_IMAGE_CREATE_SPARSE_BINDING_BIT：启用“稀疏绑定”的基础能力，允许将图像绑定到一个或多个不连续的内存块上。
		//VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT：启用“稀疏驻留”能力，允许图像在生命周期内动态地部分绑定内存。
		//后续自己学

		if (vkCreateImage(device, &createInfo, nullptr, &image) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create image");
		}

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate image memory");
		}

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	VkImageView createImageView(VkImage image, VkFormat format ,VkImageAspectFlags aspectMask ,uint32_t mipLevels)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.format = format;
		createInfo.image = image;
		createInfo.subresourceRange.aspectMask = aspectMask;
		//VK_IMAGE_ASPECT_DEPTH_BIT深度纹理
		//VK_IMAGE_ASPECT_STENCIL_BIT模板纹理（是否符合模板不符合丢弃）
		createInfo.subresourceRange.layerCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.levelCount = mipLevels;
		createInfo.subresourceRange.baseMipLevel = 0;
		/*
		* 1.mipmap是类似于生成一系列根据原图降低分辨率的图像，然后根据这些图像，在渲染不同距离的东西时直接调用
		* 节省gpu内存，有利于内存命中，还能减少画面闪烁（远处的如果实时计算，可能会在短时间有不同的样子）
		* 2.layer是这个image具有的层数（立方体的6个面），然后视图可以决定你能看到几个面，从哪个面开始。
		* 3.每个image可以拥有一个或多个layer，而每个layer可以拥有一个或多个mipmap。
		*/
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;	//取决于image，而他默认是2d，除非启用扩展

		VkImageView imageView{};
		if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create image view");
		}

		return imageView;
	}

	//改变图像布局
	void transitionImageLayout(VkImage image,VkFormat format,VkImageLayout oldLayout,VkImageLayout newLayout ,uint32_t mipLevels)
	{
		VkCommandBuffer stagingCommandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseMipLevel = 0;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (hasStencilComponent(format))
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;	//并不是真实存在的阶段，而是transfer发生的伪阶段
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			//深度测试与模板测试同时发生再光栅化后，片段着色器之前的片段测试
			//用于读取深度后判断是否写入片段值，然后写入片段
			//读取发生在VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			//写入发生在VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
		}
		else
		{
			throw std::runtime_error("unsupported layout transition");
		}

		vkCmdPipelineBarrier(stagingCommandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		//第四个参数可以用0，或者VK_DEPENDENCY_BY_REGION_BIT
		//前者表明dststage的工作要等srcstage的工作全部完成，后者表明屏障优化为区域，
		//只要自己要操作的区域的srcstage工作完成就可以把这部分开工
		
		endSingleTimeCommands(stagingCommandBuffer);
	}

	//把数据从buffer放入image
	void copyBufferToImage(VkBuffer srcBuffer,VkImage dstImage,uint32_t width,uint32_t height)
	{
		VkCommandBuffer stagingCommandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferImageHeight = 0;	//图像的高度是多少（0的情况下就是默认的图像的大小，非0，说明两张图像的上下之间有空隙）
		region.bufferRowLength = 0;		//图像的宽度是多少（0就是默认图像的大小，非0，说明每张图像每行之间有间隙）
		region.imageOffset = { 0,0,0 };
		region.imageExtent.depth = 1;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.mipLevel = 0;

		vkCmdCopyBufferToImage(stagingCommandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		endSingleTimeCommands(stagingCommandBuffer);
	}

	//创建一个临时命令缓冲区，并开始录制
	VkCommandBuffer beginSingleTimeCommands()
	{
		VkCommandBuffer stagingCommandBuffer{};

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = stagingCommandPool;

		vkAllocateCommandBuffers(device, &allocInfo, &stagingCommandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(stagingCommandBuffer, &beginInfo);

		return stagingCommandBuffer;
	}

	//结束临时命令缓冲区的录制，并提交，再销毁
	void endSingleTimeCommands(VkCommandBuffer stagingCommandBuffer)
	{
		vkEndCommandBuffer(stagingCommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &stagingCommandBuffer;
		
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkDeviceWaitIdle(device);

		vkFreeCommandBuffers(device, stagingCommandPool, 1, &stagingCommandBuffer);
	}

	//创建深度缓冲资源
	void createDepthResources()
	{
		VkFormat format = findDepthFormat();
		createImage2D(
			swapChainImageExtent.width, swapChainImageExtent.height,1,
			format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			depthImage, depthImageMemory
		);

		depthImageView = createImageView(depthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		transitionImageLayout(depthImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
		//再cmdBeginRenderPass会自动执行

	}

	//销毁深度缓冲资源
	void cleanUpDepthResources()
	{
		vkDestroyImageView(device, depthImageView, nullptr);
		vkFreeMemory(device, depthImageMemory, nullptr);
		vkDestroyImage(device, depthImage, nullptr);
	}

	//判断是否使用模板
	bool hasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	//寻找适合深度缓冲的format
	VkFormat findDepthFormat()
	{
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	//寻找合适的format
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
			
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("fail to find supported format");
	}
};



int main()
{
	TriangleApplication a;
	try {
		a.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}