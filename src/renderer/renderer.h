#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"
#include "render_types.h"
#include "core/components.h"

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
    uint32_t num_static_meshes;
    C_StaticMesh* static_meshes;

    uint32_t num_animated_meshes;
    C_AnimatedMesh* animated_meshes;
}
Scene_InitInfo;
void Renderer_ChangeScene(Scene_InitInfo new_scene_info);

#endif  // ENGINE_RENDERER_H
