#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <optional>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

//验证层声明
const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

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

	bool isComplete()
	{
		return graphicsFamily.has_value();
	}
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
		pickPhysicalDevice();
		createLogicalDevice();
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
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroyDevice(device, nullptr);
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
		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
											  deviceFeatures.geometryShader &&
											  indice.isComplete();
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

		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indice.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;       //指针是因为能为多个队列设置优先级

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 0;

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("fail to create logical device");
		}

		//第二个参数是索引（队列族索引），第三个参数也是索引（不过这个索引的创建的队列的索引，因为之创建了一个所以为0）
		vkGetDeviceQueue(device, indice.graphicsFamily.value(), 0, &graphicsQueue);
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