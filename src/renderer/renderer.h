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
void Renderer_PushRenderable(Renderable renderable);
void Renderer_DrawFrame();


typedef struct Scene_InitInfo
{
    uint32_t num_prefabs;
    Asset** prefabs;
}
Scene_InitInfo;
void Renderer_ChangeScene(Scene_InitInfo new_scene_info);

// Optional callback: called between ImGui::NewFrame() and ImGui::Render()
// Game code can set this to build its own ImGui UI
typedef void (*Renderer_ImGuiBuildCallback)(void* user_data);

void Renderer_SetImGuiCallback(Renderer_ImGuiBuildCallback callback, void* user_data);

#endif  // ENGINE_RENDERER_H
