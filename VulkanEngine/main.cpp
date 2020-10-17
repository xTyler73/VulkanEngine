#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> //#include <vulkan/vulkan.h> // commented out as GLFW will include its own definitions and load the vulkan header with it

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>

#include <vector>
#include <optional> // c++17 data structure - wrapper that contains no value until something is assigned to it. check by calling .has_value(), using to distinguish between the case of a value existing or not
#include <algorithm> // for std::min/max functions
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG // "not debug"
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static std::vector<char> readFile(const std::string& filename);

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	// have to lookup its address with vkGetInstanceProcAddr because vkCreateDebugUtilsMessengerEXT is an extension function, so it's not automatically loaded.
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT; // nullptr is returned if the function couldn't be loaded
}

// for cleanup of VkDebugUtilsMessengerEXT obj
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
		glfwInit();
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
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		createCommandBuffers();
		createSemaphores();
	}
	

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	void createSemaphores() {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

			throw std::runtime_error("Failed to create semaphores!");
		}
	}


	std::vector<VkCommandBuffer> commandBuffers;
	// allocates and records the commands for each swap chain image
	void createCommandBuffers() {
		commandBuffers.resize(swapChainFrameBuffers.size());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // can be submitted to a queue for execution, but cannot be called from other command buffers
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}

		// command buffer recording:
		for (size_t i = 0; i < commandBuffers.size(); ++i) {
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			// render pass:
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFrameBuffers[i]; // attachments to bind - a framebuffer for each swap chain image that specifies it as color attachment
			
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // for VK_ATTACHMENT_LOAD_OP_CLEAR
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // vkCmd prefix = records commands, and returns void. so no error handling until finished recording
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
		}

	}

	VkCommandPool commandPool;
	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); // record commands for drawing
		poolInfo.flags = 0;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}


	std::vector<VkFramebuffer> swapChainFrameBuffers;
	void createFrameBuffers() {
		swapChainFrameBuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
			VkImageView attachments[1] = { swapChainImageViews[i] }; // only 1 for now, the color attachment

			VkFramebufferCreateInfo frameBufferInfo{};
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = renderPass;
			frameBufferInfo.attachmentCount = 1;
			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.width = swapChainExtent.width;
			frameBufferInfo.height = swapChainExtent.height;
			frameBufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}



	VkPipeline graphicsPipeline;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	void createGraphicsPipeline() {
		std::vector<char> vertShaderCode = readFile("Shaders/vert.spv");
		std::vector<char> fragShaderCode = readFile("Shaders/frag.spv");
		//std::cout << "vertShaderCode.size: " << vertShaderCode.size() << ", fragShaderCode.size: " << fragShaderCode.size() << std::endl;

		// the compilation and the linking of SPIR-V bytecode to machine code for execution by the GPU doesn't happen until the graphics pipeline is created, so we can create these as
		// local variables because we're allowed to destroy the shader modules as soon as the pipeline creation is finished. we Destroy them at the end of this function.
		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		// Shader Stage Creation:
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main"; // the name of the entrypoint function to invoke
		//vertShaderStageInfo.pSpecializationInfo = nullptr; // done automatically anyway from struct initialization. same with for fragment shader below.

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main"; // the name of the entrypoint function to invoke

		VkPipelineShaderStageCreateInfo shaderStages[2] = { vertShaderStageInfo, fragShaderStageInfo };

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // triangle from every 3 vertices without reuse
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE; // false to discard fragments beyond far/near clipping planes. true clamps to them
		rasterizer.rasterizerDiscardEnable = VK_FALSE; // false to allow geometry to pass through the rasterizer stage
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // how the fragments are generated for geo: _FILL to fill the area with fragments, _LINE for only polygon edges (wireframe), _POINT for just the points. (last 2 req enable GPU feature)
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		// DEPTH/STENCIL BUFFER TODO - VkPipelineDepthStencilStateCreateInfo depth{}; - just leave as nullptr for now

		// specify how to combine the old value already in the frame buffer with the new returned color from the fragment shader:
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; //VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; //VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = colorBlending.blendConstants[1] = colorBlending.blendConstants[2] = colorBlending.blendConstants[3] = 0.0f;

		VkDynamicState dynamicStates[2] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}



		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages; // reference the earlier array of VkPipelineShaderStageCreateInfo structs

		//reference all of the structures describing the fixed function stage:
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr; // optional

		pipelineInfo.layout = pipelineLayout; // vulkan handle from earlier

		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0; // index of the subpass where this graphics pipeline will be used

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // optional, but good to be explicit since we're not creating a new pipeline by deriving from an existing one.
		pipelineInfo.basePipelineIndex = -1; // ^ also these values are only used if VK_PIPELINE_CREATE_DERIVATIVE_BIT is also set in this pipelineInfo.flags (VkGraphicsPipelineCreateInfo)

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}



		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	void createRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear the existing values in the attachment to a constant before the start of render
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store rendered contents in memory to be read later (at the end of render)

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // layout the image will have before the render pass, using UNDEFINED as it doesn't matter what the previous layout the image was in
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // the layout to automatically transition to when the render pass finishes

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}	
	}

	// have to wrap shader code in a VkShaderModule before we can pass it into the pipeline, they're just a thin wrapper around the shader bytecode.
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();

		// size of the bytecode is specified in bytes, but the bytecode pointer is a uint32_t pointer (not a char pointer). so cast the pointer.
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // std::vector default allocator already satisfies alignment reqs of uint32_t as well.

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule;
	}

	std::vector<VkImageView> swapChainImageViews; // to use any VkImage (including those in the swap chain) in the render pipeline, we have to create a VkImageView object for each one, so store them.
	// creates a basic image view for every image in the swap chain so that we can use them as color targets later on
	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size()); // fit all of the image views
		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // treats the image as a 2D texture. could be 1D, 2D, 3D and cube maps
			createInfo.format = swapChainImageFormat; // how the data should be interpreted

			// components allows us to swizzle colours around, eg could map all channels to red for a monochrome texture. stick to default mapping for now:
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			// subresourceRange describes what the image's purpose is, and which part of the image should be accessed. Use our images as color targets without any mipmapping levels or multiple layers:
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1; // 1 unless in a stereographic 3D app, then the swap chain would be created with multiple layers, with multiple image views for L/R eyes accessed via different layers

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages; // for storing the handles of the VkImage's in it. images were created by the implementation for the swap chain itself, so they'll get cleaned up with swapChain, without needing to clean up this
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		// specify how many images we want to have in the swap chain: .minImageCount is the minimum the implementation requires to functions
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // +1 (at least) more than the minimum so that we don't have to wait on the driver to complete internal operations before we can acquire another image to render to
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) { // make sure we don't exceed the maximum number of images though. 0 is special value that means there is no maximum
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface; // tie the swap chain to the surface

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; // the amount of layers each image consists of. is always 1 unless developing a stereoscopic 3D application
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // imageUsage specifies what kind of operations we'll use the images in the swap chain for. rendering directly to them so they're used as a color attachment
		// if we wanted to use post processing, we would use VK_IMAGE_USAGE_TRANSFER_DST_BIT to render images to a separate image first. and use a memory operation to xfer the rendered image to a swap chain image


		// specify how to handle swap chain images that will be used across different/same queue families:
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily) { // queue families differ
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // an image is used by one queue family at a time and ownership must be explcitily xferred before using it in another queue family. best perf option
			// CONCURRENT mode requires us to specify in advance (between at least 2 distinct queue families) which queue families ownership will be shared using these 2 parameters:
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else { // graphics queue family and presentation view family are the same, which will be the case on most hardware
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // images can be used across multiple queue families without explicit ownership xfers
			createInfo.queueFamilyIndexCount = 0; // optional
			createInfo.pQueueFamilyIndices = nullptr; // optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // no transformation to images in the swap chain. capabilities.supportedTransforms may support things like 90 degree CW rotation, horiz flip, etc
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // the alpha channel that should be used for blending with other windows in the window system. almost always want to ignore the alpha channel, so using OPAQUE

		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // true means that we don't care about the colour of pixels that are obscured (eg another window is in front of them). clipping gives best perf. false only if we really need to read those pixels back and get predictable results

		createInfo.oldSwapchain = VK_NULL_HANDLE; // TODO: old swap chain can become invalid/unoptimized while app is running (eg if window is resized). swap chain needs to be remade from scratch, with a reference to the old one assigned here. for now assume we only ever create 1

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}


		// we only specified a minimum number of images in the swap chain, so the implementation is allowed to create a swap chain with more. so,
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); // first query the final number of images
		swapChainImages.resize(imageCount); // resize the container
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data()); // now retrieve handles

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
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

	// Find the best possible surface format (color depth) for the swap chain when swapChainAdequate is true in isDeviceSuitable()
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats)
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) // preferred combo
				return availableFormat;
		return availableFormats[0]; // if preffered combo doesn't exit, just return first one. could add ranking logic, TODO
	}

	// Find the best possible presentation mode (conditions for "swapping" images to the screen) for the swap chain when swapChainAdequate is true in isDeviceSuitable()
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		/* possible values are:
		* VK_PRESENT_MODE_IMMEDIATE_KHR - images submitted to app are transferred to the screen right away. may cause tearing
		* VK_PRESENT_MODE_FIFO_KHR - this is v sync, when the display is refreshed it takes an image from the front of the queue and the program inserts rendered images at the back of the queue (if queue is full, program must wait)
		* VK_PRESENT_MODE_RELAXED_KHR - similar to FIFO, but if the application was late and the queue is empty at the last v blank, the image is transferred when it finally arrives. may cause tearing
		* VK_PRESENT_MODE_MAILBOX_KHR - similar to FIFO, but instead of blocking the app when the queue is full, the images that are already queued are replaced with the newer ones. can be used to implement triple buffering
		*/
		for (const auto& availablePresentMode : availablePresentModes)
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) // triple buffering to avoid tearing and have low latency by rendering new images that are as up to date as possible at the v blank
				return availablePresentMode;
		return VK_PRESENT_MODE_FIFO_KHR; // this is the only mode guaranteed to be available, so return it as a last resort
	}

	// Find the best possible swap extent (resolution of images in the swap chain) for the swap chain when swapChainAdequate is true in isDeviceSuitable()
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != UINT32_MAX) { // the resolution of the swap chain images is almost always exactly equal to the resolution of the window that we're drawing to
			return capabilities.currentExtent;
		}
		// width is == to UINT32_MAX here, which is a special value for some window managers that allow us to differ the res of the swap chain images and res of the window.
		// so clamp WIDTH and HEIGHT to the min/max extents that are supported by the implementation by picking the res that best matches the window within the minImageExtent and maxImageExtent bounds
		VkExtent2D actualExtent = { WIDTH,HEIGHT };
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;

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
			drawFrame();
		}
	}

	void drawFrame() {
		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex); // acquire image from swap chain

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[1] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[1] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[1] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		vkQueuePresentKHR(presentQueue, &presentInfo); // submit request to present an image to the swap chain
	}

	void cleanup() {
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);


		vkDestroyCommandPool(device, commandPool, nullptr);

		for (VkFramebuffer framebuffer : swapChainFrameBuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		for (VkImageView imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr); // unlike images, the image views were explicitly created by us, so have to cleanup
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);

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

#include <fstream>
static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary); // ate: start reading at end of file, binary: read as binary file to avoid text transformations
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg(); // since we started reading at the end of the file, the read position can be used to determine the size of the file for allocating the buffer
	std::vector<char> buffer(fileSize);

	// now we can seek to the beginning and read all bytes at once:
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

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