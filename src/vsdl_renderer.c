#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_log.h>
#include "vsdl_renderer.h"
#include "vsdl_types.h"
#include "vsdl_text.h"
#include "vsdl_pipeline.h"


int vsdl_init_renderer(VSDL_Context* ctx) {
  // Create triangle vertex buffer
  Vertex vertices[] = {
      {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Top, red
      {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // Bottom right, green
      {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}  // Bottom left, blue
  };

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = sizeof(vertices);
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {0};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  if (vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo, &ctx->vertexBuffer, &ctx->vertexBufferAllocation, NULL) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create vertex buffer");
      return 0;
  }

  void* data;
  vmaMapMemory(ctx->allocator, ctx->vertexBufferAllocation, &data);
  memcpy(data, vertices, sizeof(vertices));
  vmaUnmapMemory(ctx->allocator, ctx->vertexBufferAllocation);

  // Create framebuffers
  ctx->framebuffers = (VkFramebuffer*)malloc(ctx->swapchainImageCount * sizeof(VkFramebuffer));
  for (uint32_t i = 0; i < ctx->swapchainImageCount; i++) {
      VkImageView attachments[] = {ctx->swapchainImageViews[i]};
      VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      fbInfo.renderPass = ctx->renderPass;
      fbInfo.attachmentCount = 1;
      fbInfo.pAttachments = attachments;
      fbInfo.width = ctx->swapchainExtent.width;
      fbInfo.height = ctx->swapchainExtent.height;
      fbInfo.layers = 1;
      if (vkCreateFramebuffer(ctx->device, &fbInfo, NULL, &ctx->framebuffers[i]) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create framebuffer %u", i);
          return 0;
      }
  }

  // Create descriptor pool
  VkDescriptorPoolSize poolSize = {0};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(ctx->device, &poolInfo, NULL, &ctx->descriptorPool) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor pool");
      return 0;
  }

  // Allocate descriptor set
  VkDescriptorSetAllocateInfo allocInfoDS = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfoDS.descriptorPool = ctx->descriptorPool;
  allocInfoDS.descriptorSetCount = 1;
  allocInfoDS.pSetLayouts = &ctx->descriptorSetLayout;
  if (vkAllocateDescriptorSets(ctx->device, &allocInfoDS, &ctx->descriptorSet) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor set");
      return 0;
  }

  // Update descriptor set with font atlas
  VkDescriptorImageInfo imageInfo = {0};
  imageInfo.sampler = ctx->fontAtlas.sampler;
  imageInfo.imageView = ctx->fontAtlas.textureView;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  descriptorWrite.dstSet = ctx->descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(ctx->device, 1, &descriptorWrite, 0, NULL);

  // Create synchronization objects
  VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(ctx->device, &semaphoreInfo, NULL, &ctx->imageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(ctx->device, &semaphoreInfo, NULL, &ctx->renderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(ctx->device, &fenceInfo, NULL, &ctx->frameFence) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create synchronization objects");
      return 0;
  }

  SDL_Log("Renderer initialized");
  return 1;
}

void vsdl_draw_frame(VSDL_Context* ctx) {
  vkWaitForFences(ctx->device, 1, &ctx->frameFence, VK_TRUE, UINT64_MAX);
  vkResetFences(ctx->device, 1, &ctx->frameFence);

  uint32_t imageIndex;
  vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX, ctx->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

  VkCommandBufferAllocateInfo cmdAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cmdAllocInfo.commandPool = ctx->commandPool;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(ctx->device, &cmdAllocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = ctx->renderPass;
  renderPassInfo.framebuffer = ctx->framebuffers[imageIndex];
  renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
  renderPassInfo.renderArea.extent = ctx->swapchainExtent;
  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Draw triangle
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphicsPipeline);
  VkBuffer vertexBuffers[] = {ctx->vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  // Draw text
  vsdl_render_text(ctx, commandBuffer, "Hello", -0.5f, -0.5f);

  vkCmdEndRenderPass(commandBuffer);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkSemaphore waitSemaphores[] = {ctx->imageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  VkSemaphore signalSemaphores[] = {ctx->renderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, ctx->frameFence);

  VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &ctx->swapchain;
  presentInfo.pImageIndices = &imageIndex;
  vkQueuePresentKHR(ctx->graphicsQueue, &presentInfo);

  vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &commandBuffer);
}



