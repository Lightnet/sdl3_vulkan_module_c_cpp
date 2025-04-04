#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "vsdl_types.h"
#include "vsdl_pipeline.h"
#include "vsdl_text.h"

// Debug callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    const char* severityStr;
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severityStr = "VERBOSE"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severityStr = "INFO"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severityStr = "WARNING"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severityStr = "ERROR"; break;
        default: severityStr = "UNKNOWN";
    }
    SDL_Log("[Vulkan %s] %s", severityStr, pCallbackData->pMessage);
    return VK_FALSE;
}

static VkResult createDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}


int vsdl_init(VSDL_Context* ctx) {
  SDL_Log("vsdl_init SDL_Init");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
      return 0;
  }

  SDL_Log("vsdl_init SDL_CreateWindow");
  ctx->window = SDL_CreateWindow("Vulkan Triangle", 800, 600, SDL_WINDOW_VULKAN);
  if (!ctx->window) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s", SDL_GetError());
      return 0;
  }

  SDL_Log("vsdl_init volkInitialize");
  if (volkInitialize() != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Volk");
      return 0;
  }

  VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  appInfo.pApplicationName = "Vulkan SDL3";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  Uint32 extensionCount = 0;
  const char *const *baseExtensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  if (!baseExtensionNames) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get Vulkan extensions: %s", SDL_GetError());
      SDL_DestroyWindow(ctx->window);
      SDL_Quit();
      return 0;
  }

  const char** extensionNames = SDL_calloc(extensionCount + 1, sizeof(const char*));
  for (Uint32 i = 0; i < extensionCount; i++) {
      extensionNames[i] = baseExtensionNames[i];
  }
  extensionNames[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debugCreateInfo.messageSeverity = 
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugCreateInfo.messageType = 
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugCreateInfo.pfnUserCallback = debugCallback;
  debugCreateInfo.pUserData = NULL;

  SDL_Log("Found %u Vulkan instance extensions:", extensionCount);
  for (Uint32 i = 0; i < extensionCount; i++) {
      SDL_Log("  %u: %s", i + 1, extensionNames[i]);
  }

  VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = extensionCount + 1;
  createInfo.ppEnabledExtensionNames = extensionNames;
  const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
  SDL_Log("Init VK_LAYER_KHRONOS_validation");
  createInfo.enabledLayerCount = 1;
  createInfo.ppEnabledLayerNames = validationLayers;
  createInfo.pNext = &debugCreateInfo;

  SDL_Log("init vkCreateInstance");
  if (vkCreateInstance(&createInfo, NULL, &ctx->instance) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan instance");
      SDL_free(extensionNames);
      return 0;
  }
  volkLoadInstance(ctx->instance);
  SDL_Log("Vulkan instance created with %u extensions", extensionCount + 1);

  if (createDebugUtilsMessengerEXT(ctx->instance, &debugCreateInfo, NULL, &ctx->debugMessenger) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set up debug messenger");
  } else {
      SDL_Log("Debug messenger created successfully");
  }
  SDL_free(extensionNames);

  if (!SDL_Vulkan_CreateSurface(ctx->window, ctx->instance, NULL, &ctx->surface)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan surface: %s", SDL_GetError());
      return 0;
  }
  SDL_Log("Vulkan surface created");

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, NULL);
  if (deviceCount == 0) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No physical devices found");
      return 0;
  }
  VkPhysicalDevice* devices = (VkPhysicalDevice*)SDL_calloc(deviceCount, sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, devices);
  ctx->physicalDevice = devices[0];
  SDL_free(devices);
  SDL_Log("Physical device selected");

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(ctx->physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)SDL_calloc(queueFamilyCount, sizeof(VkQueueFamilyProperties));
  vkGetPhysicalDeviceQueueFamilyProperties(ctx->physicalDevice, &queueFamilyCount, queueFamilies);

  uint32_t graphicsFamily = UINT32_MAX;
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          graphicsFamily = i;
          break;
      }
  }
  SDL_free(queueFamilies);
  if (graphicsFamily == UINT32_MAX) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No graphics queue family found");
      return 0;
  }
  SDL_Log("Graphics queue family: %u", graphicsFamily);

  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  queueCreateInfo.queueFamilyIndex = graphicsFamily;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.enabledExtensionCount = 1;
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

  if (vkCreateDevice(ctx->physicalDevice, &deviceCreateInfo, NULL, &ctx->device) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan device");
      return 0;
  }
  volkLoadDevice(ctx->device);
  SDL_Log("Vulkan device created");

  VmaVulkanFunctions vulkanFunctions = {0};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  VmaAllocatorCreateInfo allocatorInfo = {0};
  allocatorInfo.physicalDevice = ctx->physicalDevice;
  allocatorInfo.device = ctx->device;
  allocatorInfo.instance = ctx->instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  if (vmaCreateAllocator(&allocatorInfo, &ctx->allocator) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create VMA allocator");
      return 0;
  }
  SDL_Log("VMA allocator created");

  vkGetDeviceQueue(ctx->device, graphicsFamily, 0, &ctx->graphicsQueue);
  ctx->graphicsFamily = graphicsFamily;
  SDL_Log("Graphics queue retrieved");

  // Create swapchain
  VkSwapchainCreateInfoKHR swapchainInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchainInfo.surface = ctx->surface;
  swapchainInfo.minImageCount = 2;
  swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  swapchainInfo.imageExtent.width = 800;
  swapchainInfo.imageExtent.height = 600;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapchainInfo.clipped = VK_TRUE;
  if (vkCreateSwapchainKHR(ctx->device, &swapchainInfo, NULL, &ctx->swapchain) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create swapchain");
      return 0;
  }
  SDL_Log("Swapchain created");

  vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->swapchainImageCount, NULL);
  ctx->swapchainImages = (VkImage*)malloc(ctx->swapchainImageCount * sizeof(VkImage));
  vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->swapchainImageCount, ctx->swapchainImages);
  ctx->swapchainExtent = swapchainInfo.imageExtent;
  ctx->swapchainImageViews = (VkImageView*)malloc(ctx->swapchainImageCount * sizeof(VkImageView));
  for (uint32_t i = 0; i < ctx->swapchainImageCount; i++) {
      VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      viewInfo.image = ctx->swapchainImages[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;
      if (vkCreateImageView(ctx->device, &viewInfo, NULL, &ctx->swapchainImageViews[i]) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create swapchain image view %u", i);
          return 0;
      }
  }
  SDL_Log("Swapchain image views created (count: %u)", ctx->swapchainImageCount);

  // Create render pass
  VkAttachmentDescription colorAttachment = {0};
  colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {0};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  if (vkCreateRenderPass(ctx->device, &renderPassInfo, NULL, &ctx->renderPass) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render pass");
      return 0;
  }
  SDL_Log("Render pass created");

  // Create command pool
  VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.queueFamilyIndex = graphicsFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(ctx->device, &poolInfo, NULL, &ctx->commandPool) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create command pool");
      return 0;
  }
  SDL_Log("Command pool created");

  // Create descriptor set layout
  VkDescriptorSetLayoutBinding layoutBinding = {0};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &layoutBinding;
  if (vkCreateDescriptorSetLayout(ctx->device, &layoutInfo, NULL, &ctx->descriptorSetLayout) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor set layout");
      return 0;
  }
  SDL_Log("Descriptor set layout created");

  // Create pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &ctx->descriptorSetLayout;
  if (vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, NULL, &ctx->pipelineLayout) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create pipeline layout");
      return 0;
  }
  SDL_Log("Pipeline layout created");

  // Pipelines and text initialization moved to vsdl_init_renderer and vsdl_init_text
  return 1;
}


