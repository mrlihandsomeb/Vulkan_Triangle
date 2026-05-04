#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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
	const VkDebugUtilsMessengerCreateInfoEXT *pCreatInfo,
	const VkAllocationCallbacks *pAllocator,
	VkDebugUtilsMessengerEXT *pDebugMessenger)
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
	const VkAllocationCallbacks * pAllocator)
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
		return graphicsFamily.has_value()&&presentFamily.has_value();
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
	VkPipelineLayout pipeLineLayout;					//管线布局，在管线创建前使用
	VkPipeline graphicsPipeLine;
	std::vector<VkFramebuffer> swapChainFramBuffers;   //帧缓冲区
	VkCommandPool commandPool;						   //命令池,用于管理命令缓冲区的内存
	VkCommandBuffer commandBuffer;					   //命令缓冲区
	VkSemaphore imageAvalableSemaphore;				   //信号量:image是否可以用
	VkSemaphore renderFinishedSemaphore;			   //信号量:image是否画完了
	VkFence inFlightFence;							   //栅栏帧:是否画完了




	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

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
		createGraphicsPipeLine();
		createFrameBuffers();
		createCommandPool();
		createCommandBuffer();
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
		vkDestroyCommandPool(device, commandPool, nullptr);
		destroyFrameBuffers();          //封装了多个vkDestroyFrameBuffer()来摧毁帧缓冲
		vkDestroyPipeline(device, graphicsPipeLine, nullptr);
		vkDestroyPipelineLayout(device, pipeLineLayout, nullptr); 
		vkDestroyRenderPass(device, renderPass, nullptr);
		destroyImageViews();			//封装了多个vkDestroyImageView()来摧毁多个视图
		vkDestroySwapchainKHR(device, swapChain, nullptr);
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
		if (vkCreateInstance(&creatInfo, nullptr, &instance)!=VK_SUCCESS)
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

		if (CreatDebugUtilsMessengerEXT(instance, &creatInfo, nullptr, &debugMessenger)!=VK_SUCCESS)
		{
			throw std::runtime_error("fail to setup debug messenger");
		}

	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &creatInfo)
	{
		creatInfo={};
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
		void * pUserData)
		//带bit的是枚举类型，不带的是普通的结构体，flag也是一种类型，第三个变量还有一个类似名字的变量
		//带flag，它类似于从位上进行操作的一系列值。
	{
		std::cerr << "validation layer:" << pcallBackData->pMessage << std::endl;
		
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
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, & isPresentSupport);

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

		uint32_t formatCount{0};
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
			if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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

		uint32_t imageCount=swapChainSupport.capabilities.minImageCount+1; 
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
		/*所用的宏是直接绘画，或者还有先将图像渲染到单独的图像中，以执行诸如后处理之类的操作*/

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),indices.presentFamily.value()};

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

	//创建图像视图
	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;		//取决于image，而他默认是2d，除非启用扩展
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	
												   //VK_IMAGE_ASPECT_DEPTH_BIT深度纹理
												   //VK_IMAGE_ASPECT_STENCIL_BIT模板纹理（是否符合模板不符合丢弃）
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i])!=VK_SUCCESS) 
			{
				throw std::runtime_error("fail to creat image views");
			}
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

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRefference;
		/*
		* pInputAttachments：从着色器读取的附件
		* pResolveAttachments：用于多重采样颜色附件的附件
		* pDepthStencilAttachment：用于深度和模板数据的附件
		* pPreserveAttachments：此子过程未使用但必须保留数据的附件
		*/

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;   //等待图像管线颜色输出阶段完成后，再开始子渲染过程
		dependency.srcAccessMask = 0;  //前一个阶段一定要完成什么，0代表啥也不干
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &colorAttachment;
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
		VkPipelineVertexInputStateCreateInfo inputCreateInfo{};
		inputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputCreateInfo.vertexAttributeDescriptionCount = 0;
		inputCreateInfo.pVertexAttributeDescriptions = nullptr;
		inputCreateInfo.vertexBindingDescriptionCount = 0;
		inputCreateInfo.pVertexBindingDescriptions = nullptr;

		
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
		rasterizationCreateInfo.depthClampEnable = VK_FALSE; //如果非默认（true）情况下会把深度值超出范围的强行放在最近的边界内
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE; //设置为 VK_TRUE，则几何体永远不会通过光栅化器阶段。
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		/*
		* VK_POLYGON_MODE_FILL：用片段填充多边形的区域
		* VK_POLYGON_MODE_LINE：多边形边缘绘制为线条
		* VK_POLYGON_MODE_POINT：多边形顶点绘制为点
		*/
		rasterizationCreateInfo.lineWidth = 1.0f;
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT; //剔除的面
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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


		//深度和模板测试(暂时传空)
		{

		}


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
		colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
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
		pipelineLayoutCreateInfo.setLayoutCount = 0;
		pipelineLayoutCreateInfo.pSetLayouts = nullptr;
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
		createInfo.pDepthStencilState = nullptr;
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
	static std::vector<char> readFile(const std::string & filename)
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
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create shaderModule");
		}

		return shaderModule;
	}

	//创建帧缓冲
	void createFrameBuffers()
	{
		swapChainFramBuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainFramBuffers.size(); ++i)
		{
			VkImageView attachment[] = { swapChainImageViews[i] };

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.width = swapChainImageExtent.width;
			createInfo.height = swapChainImageExtent.height;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = attachment;
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
	}

	//创建命令缓冲区
	void createCommandBuffer()
	{
		VkCommandBufferAllocateInfo allocInfo{};

		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		//VK_COMMAND_BUFFER_LEVEL_PRIMARY 可以提交到队列执行，但不能在其他缓冲区调用
		//VK_COMMAND_BUFFER_LEVEL_SECONDARY 不能直接提交到队列，需要在主命令缓冲区调用
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to allocate command buffer");
		}
	}

	//记录命令缓冲区的命令,并开启渲染通道
	void recordCommandBuffer(VkCommandBuffer commandBuffer,uint32_t imageIndex)
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
		VkClearValue clearColor = { {{0.0f,0.0f,0.0f,1.0f}} };
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

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

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to record command buffer");
		}
	}

	//绘画缓冲区
	void drawFrame()
	{
		vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &inFlightFence);

		uint32_t imageIndex{};
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvalableSemaphore, VK_NULL_HANDLE, &imageIndex);

		vkResetCommandBuffer(commandBuffer, 0);
		recordCommandBuffer(commandBuffer, imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphore[] = { imageAvalableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphore;
		submitInfo.pWaitDstStageMask = waitStages;
		VkSemaphore signaledSemaphore[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signaledSemaphore;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS)
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
		
		if (vkQueuePresentKHR(presentQueue, &presentInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to present queue");
		}

	}

	//创建同步对象
	void createSyncObiects()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;		//创建一个有信号的fence用来激发第一帧

		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvalableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
			vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFence) != VK_SUCCESS
			)
		{
			throw std::runtime_error("fail to create sync objects");
		}
	}

	//封装一个摧毁函数
	void destroySyncObjects()
	{
		vkDestroySemaphore(device, imageAvalableSemaphore, nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroyFence(device, inFlightFence, nullptr);
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