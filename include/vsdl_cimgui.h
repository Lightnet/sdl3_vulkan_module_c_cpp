// vsdl_cimgui.h

#ifndef VSDL_CIMGUI_H
#define VSDL_CIMGUI_H

#include "vsdl_types.h"

int vsdl_cimgui_init(VSDL_Context* ctx);
void vsdl_cimgui_new_frame(void);
void vsdl_cimgui_render(VSDL_Context* ctx, VkCommandBuffer commandBuffer);
void vsdl_cimgui_shutdown(VSDL_Context* ctx);

#endif