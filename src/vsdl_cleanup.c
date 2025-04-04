#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "vsdl_cleanup.h"
#include "vsdl_types.h"
#include <cimgui.h>
#include <cimgui_impl.h>


static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
  PFN_vkDestroyDebugUtilsMessengerEXT func = 
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != NULL) {
      func(instance, debugMessenger, pAllocator);
  }
}


void vsdl_cleanup(VSDL_Context* ctx) {
  SDL_Log("init cleanup");

  // Shut down ImGui and ensure all device operations are complete
  SDL_Log("Shutting down ImGui");
  vkWaitForFences(ctx->device, 1, &ctx->frameFence, VK_TRUE, UINT64_MAX);
  vkResetFences(ctx->device, 1, &ctx->frameFence);
  
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  igDestroyContext(NULL);

  SDL_Log("Waiting for queue to idle");
  if (ctx->device != VK_NULL_HANDLE && ctx->graphicsQueue != VK_NULL_HANDLE) {
      vkQueueWaitIdle(ctx->graphicsQueue);
  }

  // Destroy synchronization objects
  SDL_Log("Destroying frame fence");
  if (ctx->frameFence != VK_NULL_HANDLE) {
      vkDestroyFence(ctx->device, ctx->frameFence, NULL);
      ctx->frameFence = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying render finished semaphore");
  if (ctx->renderFinishedSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(ctx->device, ctx->renderFinishedSemaphore, NULL);
      ctx->renderFinishedSemaphore = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying image available semaphore");
  if (ctx->imageAvailableSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(ctx->device, ctx->imageAvailableSemaphore, NULL);
      ctx->imageAvailableSemaphore = VK_NULL_HANDLE;
  }

  // Free command buffer
  SDL_Log("Freeing command buffer");
  if (ctx->commandBuffer != VK_NULL_HANDLE && ctx->commandPool != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &ctx->commandBuffer);
      ctx->commandBuffer = VK_NULL_HANDLE;
  }

  // Destroy buffers
  SDL_Log("Destroying text vertex buffer");
  if (ctx->textVertexBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(ctx->allocator, ctx->textVertexBuffer, ctx->textVertexBufferAllocation);
      ctx->textVertexBuffer = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying vertex buffer");
  if (ctx->vertexBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(ctx->allocator, ctx->vertexBuffer, ctx->vertexBufferAllocation);
      ctx->vertexBuffer = VK_NULL_HANDLE;
  }

  // Destroy font atlas resources
  SDL_Log("Destroying font sampler");
  if (ctx->fontAtlas.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(ctx->device, ctx->fontAtlas.sampler, NULL);
      ctx->fontAtlas.sampler = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying font texture view");
  if (ctx->fontAtlas.textureView != VK_NULL_HANDLE) {
      vkDestroyImageView(ctx->device, ctx->fontAtlas.textureView, NULL);
      ctx->fontAtlas.textureView = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying font texture");
  if (ctx->fontAtlas.texture != VK_NULL_HANDLE) {
      vmaDestroyImage(ctx->allocator, ctx->fontAtlas.texture, ctx->fontAtlas.textureAllocation);
      ctx->fontAtlas.texture = VK_NULL_HANDLE;
  }
  SDL_Log("Freeing font atlas pixels");
  if (ctx->fontAtlas.pixels) {
      free(ctx->fontAtlas.pixels);
      ctx->fontAtlas.pixels = NULL;
  }
  SDL_Log("Destroying FreeType face");
  if (ctx->ftFace) {
      FT_Done_Face(ctx->ftFace);
      ctx->ftFace = NULL;
  }
  SDL_Log("Destroying FreeType library");
  if (ctx->ftLibrary) {
      FT_Done_FreeType(ctx->ftLibrary);
      ctx->ftLibrary = NULL;
  }

  // Destroy pipelines and layouts
  SDL_Log("Destroying text pipeline");
  if (ctx->textPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(ctx->device, ctx->textPipeline, NULL);
      ctx->textPipeline = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying graphics pipeline");
  if (ctx->graphicsPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(ctx->device, ctx->graphicsPipeline, NULL);
      ctx->graphicsPipeline = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying text pipeline layout");
  if (ctx->textPipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(ctx->device, ctx->textPipelineLayout, NULL);
      ctx->textPipelineLayout = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying pipeline layout");
  if (ctx->pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
      ctx->pipelineLayout = VK_NULL_HANDLE;
  }
  SDL_Log("Destroying descriptor set layout");
  if (ctx->descriptorSetLayout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);
      ctx->descriptorSetLayout = VK_NULL_HANDLE;
  }

  // Destroy descriptor pool
  SDL_Log("Destroying descriptor pool");
  if (ctx->descriptorPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
      ctx->descriptorPool = VK_NULL_HANDLE;
  }

  // Destroy framebuffers
  SDL_Log("Destroying framebuffers");
  SDL_Log("Framebuffer pointer: %p, count: %u", (void*)ctx->framebuffers, ctx->framebufferCount);
  if (ctx->framebuffers && ctx->framebufferCount > 0) {
      for (uint32_t i = 0; i < ctx->framebufferCount; i++) {
          if (ctx->framebuffers[i] != VK_NULL_HANDLE) {
              SDL_Log("Destroying framebuffer %u", i);
              vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], NULL);
              ctx->framebuffers[i] = VK_NULL_HANDLE;
          }
      }
      free(ctx->framebuffers);
      ctx->framebuffers = NULL;
      ctx->framebufferCount = 0;
  } else {
      SDL_Log("No framebuffers to destroy");
  }

  // Destroy render pass
  SDL_Log("Destroying render pass");
  if (ctx->renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(ctx->device, ctx->renderPass, NULL);
      ctx->renderPass = VK_NULL_HANDLE;
  }

  // Destroy swapchain image views
  SDL_Log("Destroying swapchain image views");
  SDL_Log("Swapchain image views pointer: %p, count: %u", (void*)ctx->swapchainImageViews, ctx->swapchainImageViewCount);
  if (ctx->swapchainImageViews && ctx->swapchainImageViewCount > 0) {
      for (uint32_t i = 0; i < ctx->swapchainImageViewCount; i++) {
          if (ctx->swapchainImageViews[i] != VK_NULL_HANDLE) {
              SDL_Log("Destroying swapchain image view %u", i);
              vkDestroyImageView(ctx->device, ctx->swapchainImageViews[i], NULL);
              ctx->swapchainImageViews[i] = VK_NULL_HANDLE;
          }
      }
      free(ctx->swapchainImageViews);
      ctx->swapchainImageViews = NULL;
      ctx->swapchainImageViewCount = 0;
  } else {
      SDL_Log("No swapchain image views to destroy");
  }

  // Destroy swapchain
  SDL_Log("Destroying swapchain");
  if (ctx->swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);
      ctx->swapchain = VK_NULL_HANDLE;
  }

  // Destroy command pool
  SDL_Log("Destroying command pool");
  if (ctx->commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(ctx->device, ctx->commandPool, NULL);
      ctx->commandPool = VK_NULL_HANDLE;
  }

  // Destroy allocator
  SDL_Log("Destroying VMA allocator");
  if (ctx->allocator != VK_NULL_HANDLE) {
      vmaDestroyAllocator(ctx->allocator);
      ctx->allocator = VK_NULL_HANDLE;
  }

  // Destroy device
  SDL_Log("Destroying device");
  if (ctx->device != VK_NULL_HANDLE) {
      vkDestroyDevice(ctx->device, NULL);
      ctx->device = VK_NULL_HANDLE;
  }

  // Destroy surface
  SDL_Log("Destroying surface");
  if (ctx->surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
      ctx->surface = VK_NULL_HANDLE;
  }

  // Destroy debug messenger
  SDL_Log("Destroying debug messenger");
  if (ctx->debugMessenger != VK_NULL_HANDLE) {
      PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
          (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
      if (destroyDebugUtilsMessenger) {
          destroyDebugUtilsMessenger(ctx->instance, ctx->debugMessenger, NULL);
      }
      ctx->debugMessenger = VK_NULL_HANDLE;
  }

  // Destroy instance
  SDL_Log("Destroying instance");
  if (ctx->instance != VK_NULL_HANDLE) {
      vkDestroyInstance(ctx->instance, NULL);
      ctx->instance = VK_NULL_HANDLE;
  }

  // Destroy window and quit SDL
  SDL_Log("Destroying window");
  if (ctx->window) {
      SDL_DestroyWindow(ctx->window);
      ctx->window = NULL;
  }
  SDL_Log("Quitting SDL");
  SDL_Quit();
}















