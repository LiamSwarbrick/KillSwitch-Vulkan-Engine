#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "SDL3/SDL.h"

/* NOTES:
For pipeline states, could use pipeline hash and use unordered maps.

Refer to https://alextardif.com/RenderingAbstractionLayers.html
also this is prolly helpful to refer to https://github.com/ravi688/VulkanRenderer/wiki/Introduction-to-V3D
*/

typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;
}
RendererInitInfo;

bool Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_OnWindowResize();

#endif  // ENGINE_RENDERER_H
