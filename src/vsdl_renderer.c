#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "vsdl_renderer.h"
#include "vsdl_types.h"
#include "vsdl_text.h"
#include "vsdl_pipeline.h"
#include <cimgui.h>
#include <cimgui_impl.h>

int vsdl_init_renderer(VSDL_Context* ctx) {
  Vertex vertices[] = {
      {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
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
  if (vmaMapMemory(ctx->allocator, ctx->vertexBufferAllocation, &data) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map vertex buffer memory");
      return 0;
  }
  memcpy(data, vertices, sizeof(vertices));
  vmaUnmapMemory(ctx->allocator, ctx->vertexBufferAllocation);

  if (!vsdl_create_graphics_pipeline(ctx)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create graphics pipeline");
      return 0;
  }

  ctx->framebuffers = (VkFramebuffer*)malloc(ctx->swapchainImageCount * sizeof(VkFramebuffer));
  if (!ctx->framebuffers) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate framebuffer array");
      return 0;
  }
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
  ctx->framebufferCount = ctx->swapchainImageCount; // Set the count here

  VkDescriptorSetAllocateInfo allocInfoDS = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfoDS.descriptorPool = ctx->descriptorPool;
  allocInfoDS.descriptorSetCount = 1;
  allocInfoDS.pSetLayouts = &ctx->descriptorSetLayout;
  if (vkAllocateDescriptorSets(ctx->device, &allocInfoDS, &ctx->descriptorSet) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor set");
      return 0;
  }

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

  VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(ctx->device, &semaphoreInfo, NULL, &ctx->imageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(ctx->device, &semaphoreInfo, NULL, &ctx->renderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(ctx->device, &fenceInfo, NULL, &ctx->frameFence) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create synchronization objects");
      return 0;
  }

  VkCommandBufferAllocateInfo cmdAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cmdAllocInfo.commandPool = ctx->commandPool;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(ctx->device, &cmdAllocInfo, &ctx->commandBuffer) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate command buffer");
      return 0;
  }

  SDL_Log("Renderer initialized");
  return 1;
}




void vsdl_draw_frame(VSDL_Context* ctx) {
  // Wait for the previous frame to finish
  VkResult result = vkWaitForFences(ctx->device, 1, &ctx->frameFence, VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to wait for frame fence: %d", result);
      return;
  }
  vkResetFences(ctx->device, 1, &ctx->frameFence);

  // Acquire the next swapchain image
  uint32_t imageIndex;
  result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX, ctx->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire next image: %d", result);
      return;
  }

  // Reset the persistent command buffer
  result = vkResetCommandBuffer(ctx->commandBuffer, 0);
  if (result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to reset command buffer: %d", result);
      return;
  }

  // Begin command buffer recording
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer(ctx->commandBuffer, &beginInfo);
  if (result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to begin command buffer: %d", result);
      return;
  }

  // Begin render pass
  VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = ctx->renderPass;
  renderPassInfo.framebuffer = ctx->framebuffers[imageIndex];
  renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
  renderPassInfo.renderArea.extent = ctx->swapchainExtent;
  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(ctx->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Draw triangle
  vkCmdBindPipeline(ctx->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphicsPipeline);
  VkBuffer vertexBuffers[] = {ctx->vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(ctx->commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdDraw(ctx->commandBuffer, 3, 1, 0, 0);

  // Draw text (optional, if keeping custom text rendering)
  vsdl_render_text(ctx, ctx->commandBuffer, "Hello", -0.5f, -0.5f);

  // ImGui frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  igNewFrame();

  // Example ImGui UI
  igBegin("Test Window", NULL, 0);
  igText("Hello, ImGui!");
  igEnd();

  igRender();
  ImDrawData* draw_data = igGetDrawData();
  ImGui_ImplVulkan_RenderDrawData(draw_data, ctx->commandBuffer, VK_NULL_HANDLE);

  // End render pass and command buffer
  vkCmdEndRenderPass(ctx->commandBuffer);
  result = vkEndCommandBuffer(ctx->commandBuffer);
  if (result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to end command buffer: %d", result);
      return;
  }

  // Submit the command buffer
  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkSemaphore waitSemaphores[] = {ctx->imageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &ctx->commandBuffer;
  VkSemaphore signalSemaphores[] = {ctx->renderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  result = vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, ctx->frameFence);
  if (result != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to submit queue: %d", result);
      return;
  }

  // Present the frame
  VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &ctx->swapchain;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(ctx->graphicsQueue, &presentInfo);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to present queue: %d", result);
      return;
  }
}


