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
	std::vector<VkImage> swapChainImage;				//这些图像是由交换链的实现创建的，它们将在交换链销毁后自动清理
	VkFormat swapChainImageFormat;						//初始化详见createSwapChain函数,不保存surfaceFormat是因为后面呢的图像试图和渲染通道只需要format，而不需要colorSpace
	VkExtent2D swapChainImageExtent;					//初始化详见createSwapChain函数




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
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
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
		swapChainImage.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImage.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainImageExtent = extent;

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