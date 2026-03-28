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


#warning NOTE(Liam): Next step is this... (see comment below)
// Now we can submit drawcalls, but...
// TODO: Call back or something for loading the meshes in a scene. Cuz of course you a Renderable has Ids of data that should be loaded into the GPU on scene load. (so meshes and materials loaded on scene load)
// I.e. on scene load, maybe we pass an array of Asset*?
// So mesh buffer resources with the FG_RESOURCE_FLAGS_SCENE_DEPENDENT flag and  MappedArena for materials that gets reset on scene change.
// void Renderer_LoadSceneResource()

#endif  // ENGINE_RENDERER_H
