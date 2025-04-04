#ifndef VSDL_RENDERER_H
#define VSDL_RENDERER_H

#include "vsdl_types.h"
// #include <cimgui.h>         // Core cimgui interface
// #include <cimgui_impl.h>    // Backend implementations (SDL3, Vulkan)

int vsdl_init_renderer(VSDL_Context* ctx);
void vsdl_draw_frame(VSDL_Context* ctx);

#endif