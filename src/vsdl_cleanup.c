#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL_log.h>
#include "vsdl_cleanup.h"
#include "vsdl_types.h"


static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
  PFN_vkDestroyDebugUtilsMessengerEXT func = 
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != NULL) {
      func(instance, debugMessenger, pAllocator);
  }
}

void vsdl_cleanup(VSDL_Context* ctx) {
  SDL_Log("init cleanup");

  // Wait for all queue operations to complete
  if (ctx->graphicsQueue) {
      SDL_Log("Waiting for queue to idle");
      vkQueueWaitIdle(ctx->graphicsQueue);
  }

  // Destroy synchronization objects
  if (ctx->frameFence) {
      SDL_Log("Destroying frame fence");
      vkDestroyFence(ctx->device, ctx->frameFence, NULL);
  }
  if (ctx->renderFinishedSemaphore) {
      SDL_Log("Destroying render finished semaphore");
      vkDestroySemaphore(ctx->device, ctx->renderFinishedSemaphore, NULL);
  }
  if (ctx->imageAvailableSemaphore) {
      SDL_Log("Destroying image available semaphore");
      vkDestroySemaphore(ctx->device, ctx->imageAvailableSemaphore, NULL);
  }

  // Destroy command buffer
  if (ctx->commandBuffer) {
      SDL_Log("Freeing command buffer");
      vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &ctx->commandBuffer);
  }

  // Destroy buffers (triangle and text)
  if (ctx->textVertexBuffer) {
      SDL_Log("Destroying text vertex buffer");
      vmaDestroyBuffer(ctx->allocator, ctx->textVertexBuffer, ctx->textVertexBufferAllocation);
  }
  if (ctx->vertexBuffer) {
      SDL_Log("Destroying vertex buffer");
      vmaDestroyBuffer(ctx->allocator, ctx->vertexBuffer, ctx->vertexBufferAllocation);
  }

  // Destroy font atlas resources (text-specific)
  if (ctx->fontAtlas.sampler) {
      SDL_Log("Destroying font sampler");
      vkDestroySampler(ctx->device, ctx->fontAtlas.sampler, NULL);
  }
  if (ctx->fontAtlas.textureView) {
      SDL_Log("Destroying font texture view");
      vkDestroyImageView(ctx->device, ctx->fontAtlas.textureView, NULL);
  }
  if (ctx->fontAtlas.texture) {
      SDL_Log("Destroying font texture");
      vmaDestroyImage(ctx->allocator, ctx->fontAtlas.texture, ctx->fontAtlas.textureAllocation);
  }
  if (ctx->fontAtlas.pixels) {
      SDL_Log("Freeing font atlas pixels");
      free(ctx->fontAtlas.pixels);
  }
  if (ctx->ftFace) {
      SDL_Log("Destroying FreeType face");
      FT_Done_Face(ctx->ftFace);
  }
  if (ctx->ftLibrary) {
      SDL_Log("Destroying FreeType library");
      FT_Done_FreeType(ctx->ftLibrary);
  }

  // Destroy pipelines (triangle and text)
  if (ctx->textPipeline) {
      SDL_Log("Destroying text pipeline");
      vkDestroyPipeline(ctx->device, ctx->textPipeline, NULL);
      ctx->textPipeline = VK_NULL_HANDLE;
  }
  if (ctx->graphicsPipeline) {
      SDL_Log("Destroying graphics pipeline");
      vkDestroyPipeline(ctx->device, ctx->graphicsPipeline, NULL);
      ctx->graphicsPipeline = VK_NULL_HANDLE;
  }

  // Destroy pipeline-related objects
  if (ctx->pipelineLayout) {
      SDL_Log("Destroying pipeline layout");
      vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
  }
  if (ctx->descriptorSetLayout) {
      SDL_Log("Destroying descriptor set layout");
      vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);
  }
  if (ctx->descriptorPool) {
      SDL_Log("Destroying descriptor pool");
      vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
  }

  // Destroy framebuffers and render pass
  if (ctx->framebuffers) {
      for (uint32_t i = 0; i < ctx->swapchainImageCount; i++) {
          if (ctx->framebuffers[i]) {
              SDL_Log("Destroying framebuffer %u", i);
              vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], NULL);
          }
      }
      free(ctx->framebuffers);
  }
  if (ctx->renderPass) {
      SDL_Log("Destroying render pass");
      vkDestroyRenderPass(ctx->device, ctx->renderPass, NULL);
  }

  // Destroy swapchain-related objects
  if (ctx->swapchainImageViews) {
      for (uint32_t i = 0; i < ctx->swapchainImageCount; i++) {
          if (ctx->swapchainImageViews[i]) {
              SDL_Log("Destroying swapchain image view %u", i);
              vkDestroyImageView(ctx->device, ctx->swapchainImageViews[i], NULL);
          }
      }
      free(ctx->swapchainImageViews);
  }
  if (ctx->swapchainImages) {
      free(ctx->swapchainImages);
  }
  if (ctx->swapchain) {
      SDL_Log("Destroying swapchain");
      vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);
  }

  // Destroy remaining device objects
  if (ctx->commandPool) {
      SDL_Log("Destroying command pool");
      vkDestroyCommandPool(ctx->device, ctx->commandPool, NULL);
  }
  if (ctx->allocator) {
      SDL_Log("Destroying VMA allocator");
      vmaDestroyAllocator(ctx->allocator);
  }
  if (ctx->device) {
      SDL_Log("Destroying device");
      vkDestroyDevice(ctx->device, NULL);
  }

  // Destroy instance-level objects
  if (ctx->surface) {
      SDL_Log("Destroying surface");
      vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
  }
  if (ctx->debugMessenger) {
      SDL_Log("Destroying debug messenger");
      destroyDebugUtilsMessengerEXT(ctx->instance, ctx->debugMessenger, NULL);
  }
  if (ctx->instance) {
      SDL_Log("Destroying instance");
      vkDestroyInstance(ctx->instance, NULL);
  }

  // Destroy SDL window
  if (ctx->window) {
      SDL_Log("Destroying window");
      SDL_DestroyWindow(ctx->window);
  }

  SDL_Log("Quitting SDL");
  SDL_Quit();
}

