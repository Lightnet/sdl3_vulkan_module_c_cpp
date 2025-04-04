#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL_log.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "vsdl_text.h"
#include "vsdl_types.h"
#include "vsdl_utils.h"


static int create_font_atlas(VSDL_Context* ctx) {
  if (FT_Init_FreeType(&ctx->ftLibrary)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize FreeType");
      return 0;
  }

  if (FT_New_Face(ctx->ftLibrary, "fonts/Kenney Mini.ttf", 0, &ctx->ftFace)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font 'Kenney Mini.ttf'");
      FT_Done_FreeType(ctx->ftLibrary);
      return 0;
  }

  FT_Set_Pixel_Sizes(ctx->ftFace, 0, 48);

  uint32_t atlasWidth = 512;
  uint32_t atlasHeight = 512;
  ctx->fontAtlas.pixels = (unsigned char*)calloc(atlasWidth * atlasHeight, sizeof(unsigned char));
  ctx->fontAtlas.width = atlasWidth;
  ctx->fontAtlas.height = atlasHeight;

  int x = 0, y = 0, maxHeight = 0;
  for (unsigned char c = 32; c < 128; c++) {
      if (FT_Load_Char(ctx->ftFace, c, FT_LOAD_RENDER)) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load glyph '%c'", c);
          continue;
      }

      if ((uint32_t)x + ctx->ftFace->glyph->bitmap.width >= atlasWidth) { // Fix signed/unsigned
          x = 0;
          y += maxHeight + 1;
          maxHeight = 0;
      }

      if ((uint32_t)y + ctx->ftFace->glyph->bitmap.rows >= atlasHeight) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Font atlas too small");
          free(ctx->fontAtlas.pixels);
          return 0;
      }

      if (!ctx->ftFace->glyph->bitmap.buffer) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Glyph '%c' has no bitmap data", c);
          continue;
      }

      for (unsigned int row = 0; row < ctx->ftFace->glyph->bitmap.rows; row++) {
          for (unsigned int col = 0; col < ctx->ftFace->glyph->bitmap.width; col++) {
              unsigned char pixel = ctx->ftFace->glyph->bitmap.buffer[row * ctx->ftFace->glyph->bitmap.width + col];
              ctx->fontAtlas.pixels[(y + row) * atlasWidth + (x + col)] = pixel;
          }
      }

      ctx->fontAtlas.glyphs[c].x = (float)x / atlasWidth;
      ctx->fontAtlas.glyphs[c].y = (float)y / atlasHeight;
      ctx->fontAtlas.glyphs[c].w = (float)ctx->ftFace->glyph->bitmap.width / atlasWidth;
      ctx->fontAtlas.glyphs[c].h = (float)ctx->ftFace->glyph->bitmap.rows / atlasHeight;
      ctx->fontAtlas.glyphs[c].advance = (float)(ctx->ftFace->glyph->advance.x >> 6);
      ctx->fontAtlas.glyphs[c].bearingX = (float)ctx->ftFace->glyph->bitmap_left;
      ctx->fontAtlas.glyphs[c].bearingY = (float)ctx->ftFace->glyph->bitmap_top;

      x += ctx->ftFace->glyph->bitmap.width + 1;
      maxHeight = (ctx->ftFace->glyph->bitmap.rows > maxHeight) ? ctx->ftFace->glyph->bitmap.rows : maxHeight;
  }

  // SDL_Log("Font atlas pixel data (first 64x64):");
  // char line[65];
  // line[64] = '\0';
  // for (int row = 0; row < 64; row++) {
  //     for (int col = 0; col < 64; col++) {
  //         unsigned char pixel = ctx->fontAtlas.pixels[row * atlasWidth + col];
  //         line[col] = (pixel > 0) ? '1' : '0';
  //     }
  //     SDL_Log("%s", line);
  // }

  // FILE* atlasFile = fopen("font_atlas.raw", "wb");
  // if (atlasFile) {
  //     fwrite(ctx->fontAtlas.pixels, 1, atlasWidth * atlasHeight, atlasFile);
  //     fclose(atlasFile);
  //     SDL_Log("Saved font atlas to font_atlas.raw (%ux%u)", atlasWidth, atlasHeight);
  // }

  VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = VK_FORMAT_R8_UNORM;
  imageInfo.extent.width = atlasWidth;
  imageInfo.extent.height = atlasHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {0};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  if (vmaCreateImage(ctx->allocator, &imageInfo, &allocInfo, &ctx->fontAtlas.texture, &ctx->fontAtlas.textureAllocation, NULL) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create font texture");
      return 0;
  }

  VkCommandBuffer cmdBuffer;
  VkCommandBufferAllocateInfo cmdAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cmdAllocInfo.commandPool = ctx->commandPool;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandBufferCount = 1;
  vkAllocateCommandBuffers(ctx->device, &cmdAllocInfo, &cmdBuffer);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuffer, &beginInfo);

  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = ctx->fontAtlas.texture;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

  VkBuffer stagingBuffer;
  VmaAllocation stagingAllocation;
  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = atlasWidth * atlasHeight;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo stagingAllocInfo = {0};
  stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  vmaCreateBuffer(ctx->allocator, &bufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

  void* data;
  vmaMapMemory(ctx->allocator, stagingAllocation, &data);
  memcpy(data, ctx->fontAtlas.pixels, atlasWidth * atlasHeight);
  vmaUnmapMemory(ctx->allocator, stagingAllocation);

  VkBufferImageCopy copyRegion = {0};
  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.layerCount = 1;
  copyRegion.imageExtent.width = atlasWidth;
  copyRegion.imageExtent.height = atlasHeight;
  copyRegion.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, ctx->fontAtlas.texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

  vkEndCommandBuffer(cmdBuffer);

  VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence;
  vkCreateFence(ctx->device, &fenceInfo, NULL, &fence);

  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuffer;
  vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, fence);
  vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);

  vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &cmdBuffer);
  vmaDestroyBuffer(ctx->allocator, stagingBuffer, stagingAllocation);
  vkDestroyFence(ctx->device, fence, NULL);

  VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = ctx->fontAtlas.texture;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  if (vkCreateImageView(ctx->device, &viewInfo, NULL, &ctx->fontAtlas.textureView) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create font texture view");
      return 0;
  }

  VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  if (vkCreateSampler(ctx->device, &samplerInfo, NULL, &ctx->fontAtlas.sampler) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create font sampler");
      return 0;
  }

  SDL_Log("Font atlas created");
  return 1;
}


int vsdl_init_text(VSDL_Context* ctx) {
    if (!create_font_atlas(ctx)) {
        return 0;
    }

    VkDescriptorSetLayoutBinding layoutBinding = {0};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;
    if (vkCreateDescriptorSetLayout(ctx->device, &layoutInfo, NULL, &ctx->descriptorSetLayout) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor set layout for text");
        return 0;
    }

    VkDescriptorPoolSize poolSize = {0};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(ctx->device, &poolInfo, NULL, &ctx->descriptorPool) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor pool for text");
        return 0;
    }

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = ctx->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &ctx->descriptorSetLayout;
    if (vkAllocateDescriptorSets(ctx->device, &allocInfo, &ctx->descriptorSet) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor set for text");
        return 0;
    }

    VkDescriptorImageInfo imageInfo = {0};
    imageInfo.sampler = ctx->fontAtlas.sampler;
    imageInfo.imageView = ctx->fontAtlas.textureView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = ctx->descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(ctx->device, 1, &descriptorWrite, 0, NULL);

    SDL_Log("Text rendering initialized");
    return 1;
}



int vsdl_create_text_pipeline(VSDL_Context* ctx) {
  size_t vertSize, fragSize;
  char* vertCode = readFile("shaders/text.vert.spv", &vertSize);
  char* fragCode = readFile("shaders/text.frag.spv", &fragSize);
  if (!vertCode || !fragCode) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load text shaders");
      free(vertCode);
      free(fragCode);
      return 0;
  }

  VkShaderModule vertModule, fragModule;
  VkShaderModuleCreateInfo vertInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  vertInfo.codeSize = vertSize;
  vertInfo.pCode = (uint32_t*)vertCode;
  VkShaderModuleCreateInfo fragInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  fragInfo.codeSize = fragSize;
  fragInfo.pCode = (uint32_t*)fragCode;

  if (vkCreateShaderModule(ctx->device, &vertInfo, NULL, &vertModule) != VK_SUCCESS ||
      vkCreateShaderModule(ctx->device, &fragInfo, NULL, &fragModule) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text shader modules");
      free(vertCode);
      free(fragCode);
      return 0;
  }

  VkPipelineShaderStageCreateInfo shaderStages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}
  };
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = vertModule;
  shaderStages[0].pName = "main";
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = fragModule;
  shaderStages[1].pName = "main";

  VkVertexInputBindingDescription bindingDesc = {0};
  bindingDesc.binding = 0;
  bindingDesc.stride = sizeof(TextVertex);
  bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attribDescs[2] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, pos)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, texCoord)}
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
  vertexInputInfo.vertexAttributeDescriptionCount = 2;
  vertexInputInfo.pVertexAttributeDescriptions = attribDescs;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {0.0f, 0.0f, (float)ctx->swapchainExtent.width, (float)ctx->swapchainExtent.height, 0.0f, 1.0f};
  VkRect2D scissor = {{0, 0}, ctx->swapchainExtent};
  VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;

  VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = ctx->pipelineLayout;
  pipelineInfo.renderPass = ctx->renderPass; // Use render pass
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &ctx->textPipeline) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text pipeline");
      vkDestroyShaderModule(ctx->device, fragModule, NULL);
      vkDestroyShaderModule(ctx->device, vertModule, NULL);
      free(vertCode);
      free(fragCode);
      return 0;
  }

  vkDestroyShaderModule(ctx->device, fragModule, NULL);
  vkDestroyShaderModule(ctx->device, vertModule, NULL);
  free(vertCode);
  free(fragCode);

  SDL_Log("Text pipeline created");
  return 1;
}



void vsdl_render_text(VSDL_Context* ctx, VkCommandBuffer commandBuffer, const char* text, float x, float y) {
  size_t len = strlen(text);
  TextVertex* vertices = (TextVertex*)malloc(len * 6 * sizeof(TextVertex));
  uint32_t vertexCount = 0;

  float scale = 1.0f;
  float cursorX = x;

  for (size_t i = 0; i < len; i++) {
      unsigned char c = (unsigned char)text[i];
      if (c < 32 || c >= 128) continue;

      GlyphMetrics* glyph = &ctx->fontAtlas.glyphs[c];
      if (glyph->w == 0 || glyph->h == 0) continue;

      float xPos = cursorX + glyph->bearingX / (float)ctx->swapchainExtent.width * 2.0f;
      float yPos = y - glyph->bearingY / (float)ctx->swapchainExtent.height * 2.0f;
      float w = glyph->w * ctx->fontAtlas.width / (float)ctx->swapchainExtent.width * 2.0f * scale;
      float h = glyph->h * ctx->fontAtlas.height / (float)ctx->swapchainExtent.height * 2.0f * scale;

      float texX = glyph->x;
      float texY = glyph->y;
      float texW = glyph->w;
      float texH = glyph->h;

      // SDL_Log("Rendering '%c': pos=(%f, %f), size=(%f, %f), tex=(%f, %f, %f, %f)", 
      //         c, xPos, yPos, w, h, texX, texY, texX + texW, texY + texH);

      vertices[vertexCount++] = (TextVertex){{xPos, yPos}, {texX, texY}};
      vertices[vertexCount++] = (TextVertex){{xPos + w, yPos}, {texX + texW, texY}};
      vertices[vertexCount++] = (TextVertex){{xPos + w, yPos + h}, {texX + texW, texY + texH}};
      vertices[vertexCount++] = (TextVertex){{xPos, yPos}, {texX, texY}};
      vertices[vertexCount++] = (TextVertex){{xPos + w, yPos + h}, {texX + texW, texY + texH}};
      vertices[vertexCount++] = (TextVertex){{xPos, yPos + h}, {texX, texY + texH}};

      cursorX += glyph->advance / (float)ctx->swapchainExtent.width * 2.0f * scale;
  }

  VkDeviceSize bufferSize = vertexCount * sizeof(TextVertex);
  if (ctx->textVertexBuffer) {
      vmaDestroyBuffer(ctx->allocator, ctx->textVertexBuffer, ctx->textVertexBufferAllocation);
  }

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = bufferSize;
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {0};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  if (vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo, &ctx->textVertexBuffer, &ctx->textVertexBufferAllocation, NULL) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text vertex buffer");
      free(vertices);
      return;
  }

  void* data;
  vmaMapMemory(ctx->allocator, ctx->textVertexBufferAllocation, &data);
  memcpy(data, vertices, bufferSize);
  vmaUnmapMemory(ctx->allocator, ctx->textVertexBufferAllocation);
  free(vertices);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->textPipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelineLayout, 0, 1, &ctx->descriptorSet, 0, NULL);

  VkBuffer vertexBuffers[] = {ctx->textVertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}