//#include <vulkan/vulkan.h> // commented out as GLFW will include its own definitions and load the vulkan header with it
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <optional> // c++17 data structure - wrapper that contains no value until something is assigned to it. check by calling .has_value(), using to distinguish between the case of a value existing or not

#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG // "not debug" - part of the c++ standard
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// proxy function
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	// have to lookup its address with vkGetInstanceProcAddr because vkCreateDebugUtilsMessengerEXT is an extension function, so it's not automatically loaded.
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT; // nullptr is returned if the function couldn't be loaded
}

// proxy function for cleanup of VkDebugUtilsMessengerEXT obj
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	void initWindow() {
		glfwInit(); // init the GLFW library
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW was originally designed to create an OpenGL context, so specifically tell it not to
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // don't allow resize for now, but it's TODO
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Window", nullptr, nullptr); // 4th param = monitor to open window on, 5th param only relevant to OpenGL
	}

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	void initVulkan() {
		createInstance(); // the instance is the connection between the application and the Vulkan library. we must specify some details about the application to the driver
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
	}

	VkDevice device; // logical device handle to interface with physicalDevice
	VkQueue graphicsQueue; // queues are automatically created along with the logical device, but still need a handle to interface with the graphics queue. device queues implicitly cleaned up when device is destroyed, so no cleanup necessary
	VkQueue presentQueue;
	void createLogicalDevice() { // sets up logical device and queue handles so that we can actually use the GPU
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f; // influences the scheduling of command buffer execution (from 0.0f to 1.0f) - required even if there is only a single queue
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{}; // this struct describes the number of queues we want for a single queue family
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{}; // atm we don't need anything special, so just define it

		VkDeviceCreateInfo createInfo{}; // with queueCreateInfo and deviceFeatures now declared, we can start filling out DeviceCreateInfo
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;

		// similar to VkInstanceCreateInfo - we must specify extensions and validation layers.
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // enable the "VK_KHR_swapchain" extension
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // newer versions of Vulkan = there is no longer a distinction between instance and device specific validation layers,
			createInfo.ppEnabledLayerNames = validationLayers.data(); // so these 2 fields of VkDeviceCreateInfo are ignored, but set them anyway to be compatible with older implementations
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue); // retrieves queue handles for each queue family. passing in 0 for queue index because we're only creating a single queue from this family
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue); // if the queue families are the same, the two queue handles likely have the same value now
	}

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // implicitly destroyed when the VkInstance instance is destroyed, so don't need to do anything in cleanup()
	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("failed to find any GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); // allocate array to hold all the VkPhysicalDevice handles
		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find any suitable!"); // GPU had vulkan support, but didn't support all Vulkan features that we use
		}
	}

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;	// can't use uint32_t, because in theory any value could be a valid queue family index, so no special value to determine the nonexistence of a queue family works
		std::optional<uint32_t> presentFamily; // vulkan implementation may support WSI, but doesn't necessarily mean that every device in the system supports it

		bool isComplete() { // generic check to the struct itself for convenience
			return graphicsFamily.has_value() && presentFamily.has_value(); // && because it is possible that the queue families supporting drawing commands and ones supporting presentation do not overlap
		}
	};

	// checks if the GPU that supports Vulkan also supports the features that we want to use
	bool isDeviceSuitable(VkPhysicalDevice device) {
		/*VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties); // gets basic device properties like name, type, supported Vulkan version, etc
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures); // many more optional features like texture compression 64 bit floats, multi viewport rendering, etc
		std::cout << deviceProperties.deviceName;*/
		// could sort all GPUs by some score and pick best one, or allow user to choose.


		QueueFamilyIndices indices = findQueueFamilies(device);
		
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		// swap chain is sufficient enough for us if there is at least one supported image format and one supported presentation mode given the window surface we have:
		bool swapChainAdequate = false;
		if (extensionsSupported) { // important to only query for swap chain support after verifying the extension is available
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	const std::vector<const char*> deviceExtensions = { // list of required device extensions, similar to the list of validation layers we want to enable
			VK_KHR_SWAPCHAIN_EXTENSION_NAME // "VK_KHR_swapchain"
	};

	// checks to make sure GPU is capable of creating a swap chain. by default the availability of a presentation queue implies that the swap chain extension must be supported, but still good to be explicit - we do still have to enable the extension regardless tho
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName); // enumerate the extensions and check if all of the requied extensions are amongst them
		}
		return requiredExtensions.empty();
	}

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities; // basic surface capabilities - min/max number of images in a swap chain, min/max width and height of images
		std::vector<VkSurfaceFormatKHR> formats; // surface formats - pixel format, color space
		std::vector<VkPresentModeKHR> presentModes; // available presentation modes
	};

	// populates and returns SwapChainSupportDetails struct with supported image formats/supported presentation modes (if any)
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities); // takes the VkPhysicalDevice and VkSurfaceKHR into account when determining the supported capabilites. all support querying functions have these 2 params as they're core components of the swap chain
		
		// query supported surface formats:
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount); // resize vector to hold all available formats
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// query supported presentation modes:
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount) {
			details.presentModes.resize(presentModeCount); // resize vector to hold all available formats
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details; // all details are in the struct now for isDeviceSuitable()
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		// Find queue family indices to populate struct with:
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		//VkQueueFamilyProperties struct contains some details about the queue family, including the type of operations that are supported, and number of queues that can be created based on that family
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) { // we need to find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT
				indices.graphicsFamily = i;
			}

			// present and graphics are very likely to be the same queue family, but treat them as separate queues for a uniform approach. could make this only prefer a physical device that supports
			// both drawing and presentation in the same queue for improved perf, but meh
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport); // look for a queue family that has the capability of presenting to our window surface
			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}
			++i;
		}
		return indices;
	}


	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // all except SEVERITY_INFO_BIT_EXT to leave out verbose general debug info
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // all
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr; // optionally passsed along to the callback function, eg could pass in a pointer to HelloTriangleApplication
		// see https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VK_EXT_debug_utils for more possibilities about ways to configure validation layer messages / debug callbacks
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers)
			return;
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		// try to create the extension object if it's available
		// debug messenger is specific to our Vulkan instance and its layers, it needs to be explicitly specified as the first arg. (similar pattern with other child objs)
		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to setup debug messenger!");
		}
	}

	void createInstance() {
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{}; // filling out the VkApplicationInfo struct is technically optional, but may provide some info to the driver in order to optimize our specific application
		// appInfo.pNext = nullptr; // is nullptr by default -- pNext will point to extension information in the future
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;


		VkInstanceCreateInfo createInfo{}; // this struct is not optional and tells the Vulkan driver which global extensions and validation layers we want to use.
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;


		/*uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // returns the Vulkan instance extensions required by GLFW, since Vulkan is a platform agnostic API
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;*/
		std::vector<const char*> extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo; // out of if scope, so that it exists for the vkCreateInstance below. By creating an additional debug messenger like this,
		// it will automatically be used during vkCreateInstance and vkDestroyInstance and cleaned up after that.
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		} else {
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}

		// general pattern for vk obj creation: (pointer to struct with creation info, pointer to custom allocator callbacks (nullptr for simplicity), pointer to variable that stores the handle to new obj)
		//VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		// nearly all Vulkan functions return a value of type VkResult, can use this to check if ins was created successfully. don't need to store store the result as a variable:
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ins!");
		}

		// could do this before creating the instance to avoid VK_ERROR_EXTENSION_NOT_PRESENT and terminate for essential functions like window system interface
		// but we can also use this to check for optional functionality and to provide some details about the Vulkan support on the device
		/*
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		std::cout << "available extensions:\n";
		for (const auto& extension : extensions) {
			std::cout << '\t' << extension.extensionName << '\n'; // also contains extension.specVersion
		}
		*/
	}

	bool checkValidationLayerSupport() { // checks if all of the requested layers are available
		//usage is identical to vkEnumerateInstanceExtensionProperties when checking for the optional functionalities
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()); // lists all of the available layers

		//check if all of the layers in our defined validationLayers variable exist in the availableLayers:
		for (const char* layerName : validationLayers) {
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers) {
				//std::cout << layerName << " : to : " << layerProperties.layerName << std::endl;
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}
			if (!layerFound) {
				return false;
			}
		}
		return true;
	}

	// returns the required list of extensions based on whether validation layers are enabled or not
	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionsCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount); // the extensions specified by GLFW are always required

		if (enableValidationLayers) { // not required, conditionally adds the debug messenger extension
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // macro is equivalent to the literal "VK_EXT_debug_utils" - but this avoids typos
		}
		return extensions;
	}

	// returns a bool that indicates if the Vulkan call that triggered that validation layer message should be aborted. if true, the call is aborted with VK_ERROR_VALIDATION_FAILED_EXT error
	// normally only used to test validation layers themselves though, so always returning VK_FALSE
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, // enum of the form: VK_DEBUG_UTILS_MESSAGE_SEVERITY_XXX, where can use >= to determine if message is important enough to show
		VkDebugUtilsMessageTypeFlagsEXT messageType, // enum of the form: VK_DEBUG_UTILS_MESSAGE_TYPE_XXX
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // refers to a VkDebugUtilsMessengerCallbackDataEXT struct containing details of the message itself, eg pMessage, pObjects, objectCount
		void* pUserData) { // pointer that was specified during the setup of the callback and allows passing of own data to it

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	VkSurfaceKHR surface;
	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface - glfw!");
		}
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) { // run app until either error occurs or window is closed
			glfwPollEvents();
		}
	}

	void cleanup() {
		vkDestroyDevice(device, nullptr); // the logical device that was interfacing with the physical device

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr); // comment out to test validation layers. also c\vulkansdk\config\vk_layer_settings.txt explains how to configure layers more than just flags
		}

		vkDestroySurfaceKHR(instance, surface, nullptr); // GLFW doesn't offer a special funtion for destroying the surface - do through original vk

		vkDestroyInstance(instance, nullptr); // instance should only be destroyed right before the program exits. all other Vulkan resources should be cleaned up before this instance itself!
		glfwDestroyWindow(window); // cleanup resources by destroying window
		glfwTerminate(); // terminate GLFW itself
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/*

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

int main() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::cout << extensionCount << " extensions supported\n";

	glm::mat4 matrix;
	glm::vec4 vec;
	auto test = matrix * vec;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();


	/*std::cout << "hi" << std::endl;
	int n;
	std::cin >> n;

	return 0;
}*/