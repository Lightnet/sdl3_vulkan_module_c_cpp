#ifndef VSDL_TYPES_H
#define VSDL_TYPES_H

#include <SDL3/SDL.h>
// #define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
// #include <volk.h>
#include <vk_mem_alloc.h>
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct {
    float pos[2];  // x, y
    float color[3]; // r, g, b
} Vertex;

typedef struct {
  float pos[3];  // 3D position
  float color[3];
} Vertex3D;  // For 3D objects

typedef struct {
    float pos[2];     // Position (x, y)
    float texCoord[2]; // Texture coordinates (u, v)
} TextVertex;

typedef struct {
  float x, y;       // Position in atlas (normalized 0–1)
  float w, h;       // Size in atlas (normalized 0–1)
  float advance;    // Advance width (pixels)
  float bearingX;   // Left bearing (pixels)
  float bearingY;   // Top bearing (pixels)
} GlyphMetrics;

typedef struct {
    VkImage texture;
    VmaAllocation textureAllocation;
    VkImageView textureView;
    VkSampler sampler;
    unsigned char* pixels;
    uint32_t width;
    uint32_t height;
    GlyphMetrics glyphs[128]; // Metrics for ASCII 32–127
} FontAtlas;

typedef struct {
    SDL_Window* window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t graphicsFamily;
    VmaAllocator allocator;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkImage* swapchainImages;
    uint32_t swapchainImageCount;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    VkImageView* swapchainImageViews;
    uint32_t swapchainImageViewCount;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;  // For triangle
    VkPipeline textPipeline;      // For text
    VkPipelineLayout textPipelineLayout;     // For the text pipeline

    VkDescriptorSetLayout descriptorSetLayout;  // For text texture
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkFramebuffer* framebuffers;
    uint32_t framebufferCount;
    VkCommandPool commandPool;
    VkBuffer vertexBuffer;              // For triangle
    VmaAllocation vertexBufferAllocation;
    VkBuffer textVertexBuffer;          // For text
    VmaAllocation textVertexBufferAllocation;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence frameFence;
    VkDebugUtilsMessengerEXT debugMessenger;
    FT_Library ftLibrary;
    FT_Face ftFace;
    FontAtlas fontAtlas;
    VkCommandBuffer commandBuffer;
    VkDescriptorPool imguiDescriptorPool; // Optional, if not using ctx->descriptorPool
} VSDL_Context;

#endif