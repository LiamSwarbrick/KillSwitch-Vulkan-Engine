#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"
#include "render_types.h"

#warning TEMP: In renderer.cpp I'm including assetsys to temporarily pass a Mesh to Renderer_InitInfo
#include "core/assetsys.h"

typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;

    // TEMP:
    Mesh* temp_test_mesh;
}
RendererInitInfo;

void Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_ListenToWindowEvent(SDL_Event event);
void Renderer_DrawFrame();

#endif  // ENGINE_RENDERER_H
