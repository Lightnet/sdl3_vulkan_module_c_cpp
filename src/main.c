#include <SDL3/SDL.h>
#include "vsdl_init.h"
#include "vsdl_renderer.h"
#include "vsdl_cleanup.h"

int main(int argc, char* argv[]) {
  SDL_Log("init main");
  VSDL_Context ctx = {0};
  if (!vsdl_init(&ctx)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize VSDL");
      vsdl_cleanup(&ctx);
      return 1;
  }

  if (!vsdl_init_text(&ctx)) { // Initialize text first (creates font atlas and text pipeline)
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize text rendering");
      vsdl_cleanup(&ctx);
      return 1;
  }

  if (!vsdl_init_renderer(&ctx)) { // Then renderer (uses font atlas for descriptor set)
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize renderer");
      vsdl_cleanup(&ctx);
      return 1;
  }

  SDL_Log("init loop");
  SDL_Log("Starting render loop");
  SDL_Event event;
  int running = 1;
  while (running) {
      while (SDL_PollEvent(&event)) {
          if (event.type == SDL_EVENT_QUIT) running = 0;
      }
      vsdl_draw_frame(&ctx);
  }

  vsdl_cleanup(&ctx);
  SDL_Log("Program ran successfully!");
  return 0;
}