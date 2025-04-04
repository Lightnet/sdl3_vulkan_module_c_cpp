#ifndef VSDL_TEXT_H
#define VSDL_TEXT_H
#include "vsdl_types.h"

int vsdl_init_text(VSDL_Context* ctx);
int vsdl_create_text_pipeline(VSDL_Context* ctx);
void vsdl_render_text(VSDL_Context* ctx, VkCommandBuffer commandBuffer, const char* text, float x, float y);

#endif