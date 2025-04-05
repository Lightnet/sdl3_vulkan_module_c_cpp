// vsdl_cimgui.c
#include "vsdl_cimgui.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cimgui.h>
#include <cimgui_impl.h>
//#include "vsdl_types.h"

static void checkVkResult(VkResult err) {
    if (err != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Vulkan error in ImGui: %d", err);
    }
}

int vsdl_cimgui_init(VSDL_Context* ctx) {
    SDL_Log("Initializing ImGui");

    // Create ImGui context
    SDL_Log("Creating ImGui context");
    igCreateContext(NULL);
    
    // Verify ImGui IO
    ImGuiIO* io = igGetIO();
    if (!io) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to retrieve ImGui IO");
        return 0;
    }

    // Initialize SDL3 backend
    SDL_Log("Initializing ImGui SDL3 backend");
    if (!ImGui_ImplSDL3_InitForVulkan(ctx->window)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize ImGui SDL3 backend");
        igDestroyContext(NULL);
        return 0;
    }

    // Initialize Vulkan backend
    SDL_Log("Setting up ImGui Vulkan backend");
    ImGui_ImplVulkan_InitInfo init_info = {0};
    init_info.Instance = ctx->instance;
    init_info.PhysicalDevice = ctx->physicalDevice;
    init_info.Device = ctx->device;
    init_info.QueueFamily = ctx->graphicsFamily;
    init_info.Queue = ctx->graphicsQueue;
    init_info.DescriptorPool = ctx->descriptorPool;
    init_info.RenderPass = ctx->renderPass;
    init_info.MinImageCount = 2;
    init_info.ImageCount = ctx->swapchainImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.Allocator = NULL;
    init_info.CheckVkResultFn = checkVkResult;

    SDL_Log("Calling ImGui_ImplVulkan_Init");
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize ImGui Vulkan backend");
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        return 0;
    }

    // Create fonts texture
    SDL_Log("Creating ImGui fonts texture");
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create ImGui fonts texture");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        return 0;
    }
    SDL_Log("ImGui fonts texture created");

    SDL_Log("ImGui initialized");
    return 1;
}

void vsdl_cimgui_new_frame(void) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();
}

void vsdl_cimgui_render(VSDL_Context* ctx, VkCommandBuffer commandBuffer) {
    igRender();
    ImDrawData* draw_data = igGetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer, VK_NULL_HANDLE);
}

void vsdl_cimgui_shutdown(VSDL_Context* ctx) {
    SDL_Log("Shutting down ImGui");
    if (ctx->device != VK_NULL_HANDLE && ctx->frameFence != VK_NULL_HANDLE) {
        vkWaitForFences(ctx->device, 1, &ctx->frameFence, VK_TRUE, UINT64_MAX);
        vkResetFences(ctx->device, 1, &ctx->frameFence);
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    igDestroyContext(NULL);
}