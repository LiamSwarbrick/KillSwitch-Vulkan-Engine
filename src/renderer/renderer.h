#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"
#include "render_types.h"

typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;
}
RendererInitInfo;

void Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_ListenToWindowEvent(SDL_Event event);

// NOTE: Returns false when swapchain invalidated some how.
bool Renderer_DrawFrame(RenderView* render_view);

#endif  // ENGINE_RENDERER_H
