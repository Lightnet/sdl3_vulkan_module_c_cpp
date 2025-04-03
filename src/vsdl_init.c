#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "vsdl_types.h"

// Debug callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData) {
  
  const char* severityStr;
  switch (messageSeverity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
          severityStr = "VERBOSE";
          break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
          severityStr = "INFO";
          break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
          severityStr = "WARNING";
          break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
          severityStr = "ERROR";
          break;
      default:
          severityStr = "UNKNOWN";
  }

  SDL_Log("[Vulkan %s] %s", severityStr, pCallbackData->pMessage);
  return VK_FALSE;
}

// Add this helper function to create the debug messenger
static VkResult createDebugUtilsMessengerEXT(VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDebugUtilsMessengerEXT* pDebugMessenger) {
  PFN_vkCreateDebugUtilsMessengerEXT func = 
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkCreateDebugUtilsMessengerEXT");
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

    SDL_Log("init VkApplicationInfo");
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Vulkan SDL3";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Get the count of required Vulkan instance extensions
    Uint32 extensionCount = 0;
    const char *const *baseExtensionNames  = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!baseExtensionNames ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get Vulkan extensions: %s", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        return 1;
    }

    // Create new extension array with debug utils
    const char** extensionNames = SDL_calloc(extensionCount + 1, sizeof(const char*));
    for (Uint32 i = 0; i < extensionCount; i++) {
        extensionNames[i] = baseExtensionNames[i];
    }
    extensionNames[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    // Debug messenger create info
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

    // Log all extensions
    SDL_Log("Found %u Vulkan instance extensions:", extensionCount);
    for (Uint32 i = 0; i < extensionCount; i++) {
      SDL_Log("  %u: %s", i + 1, extensionNames[i]);
    }

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount + 1;
    createInfo.ppEnabledExtensionNames = extensionNames;
    SDL_Log("Init VK_LAYER_KHRONOS_validation");
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
    createInfo.pNext = &debugCreateInfo;  // Chain the debug info

    SDL_Log("init vkCreateInstance");
    if (vkCreateInstance(&createInfo, NULL, &ctx->instance) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan instance");
        return 0;
    }
    volkLoadInstance(ctx->instance);
    SDL_Log("Vulkan instance created with %u extensions", 2);

    // Create debug messenger
    if (createDebugUtilsMessengerEXT(ctx->instance, &debugCreateInfo, NULL, &ctx->debugMessenger) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set up debug messenger");
      // Not a fatal error, continue execution
    } else {
        SDL_Log("Debug messenger created successfully");
    }

    SDL_free(extensionNames);


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
        vkDestroyDevice(ctx->device, NULL);
        return 0;
    }
    SDL_Log("VMA allocator created");

    vkGetDeviceQueue(ctx->device, graphicsFamily, 0, &ctx->graphicsQueue);
    ctx->graphicsFamily = graphicsFamily;
    SDL_Log("Graphics queue retrieved");

    return 1;
}