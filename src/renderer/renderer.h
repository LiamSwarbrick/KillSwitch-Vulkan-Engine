#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"
#include "impl/vulkan_wrapper.h"
#include "framegraph.h"

// NOTE(Liam): Currently implementing the Renderer backend with
// a 'quick and dirty' vulkan implementation based on something i already wrote.
// PROBLEM:
//  Every rendering feature and new renderpass has their own requirements for buffers, descriptors, samplers etc..
//  and it takes a HUGE amount of boiler plate during init and drawing for every new feature.
//  I would like to have a way to fix this, e.g. with a frame graph approach that automates this.
//  However, making sure synchronisation is close to optimal while doing this would make such a
//  frame-graph system an enormous undertaking.
// So maybe instead, I'd like to:
// - Improve statelessness and modularity of the renderer modules with respect to other modules
//   i.e. it takes in data to render each frame and that's all it care about.
//   (pipeilne states and things could be made on the fly with a hash-table lookup system)
// - Descriptor set layouts, descriptors and updating those, should be as automated as possible,
//   however this should be balanced on making sure the it isn't too complicated to design and implement.
// - Adding postfx should be made easier too, but this also isn't trivial since each postfx-shader
//   can happen at different places in the rendergraph, and the shaders use different descriptors,
//   and chaining postfx requires alternating between reading and writing between 2 'pingpong buffers'.

/* Targets for rewrite:
- The single transfer pool used for one time copy commands needs replacing.
  The renderer after the rewrite must be stateless, as in, all the render commands each frame
  are sent each time, this way, the rendering can happen during loading on another thread.
- Actually use the GPU pointer (buffer device address) extension I'm enabling (for the device and in VMA),
  and see if descriptors can be made more redundant.
- Remember that timeline semaphores are useful for multithreading.
*/

/* NOTES For Rewrite:
For pipeline states, could use pipeline hash and use unordered maps.

Refer to https://alextardif.com/RenderingAbstractionLayers.html
also this is prolly helpful to refer to https://github.com/ravi688/VulkanRenderer/wiki/Introduction-to-V3D
*/


typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;
    void (*resources_create_callback)(FG_ResourceFlags res_types_to_create_or_recreate);
}
RendererInitInfo;

bool Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_ListenToWindowEvent(SDL_Event event);

VkExtent2D Renderer_GetSwapchainExtent();

void Renderer_BeginFrame();
void Renderer_EndFrame();


#endif  // ENGINE_RENDERER_H
