#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL_log.h>
#include "vsdl_mesh.h"
#include "vsdl_types.h"

int create_vertex_buffer(VSDL_Context* ctx) {
    Vertex vertices[] = {
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Top: Red
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // Bottom right: Green
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}  // Bottom left: Blue
    };
    VkDeviceSize bufferSize = sizeof(vertices);

    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {0};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer vertexBuffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo, &vertexBuffer, &allocation, NULL) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create vertex buffer");
        return 0;
    }

    void* data;
    vmaMapMemory(ctx->allocator, allocation, &data);
    memcpy(data, vertices, bufferSize);
    vmaUnmapMemory(ctx->allocator, allocation);

    ctx->vertexBuffer = vertexBuffer;
    ctx->vertexBufferAllocation = allocation;
    SDL_Log("Vertex buffer created with VMA");

    return 1;
}