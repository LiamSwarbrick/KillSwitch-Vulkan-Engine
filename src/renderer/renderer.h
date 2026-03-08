#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"

typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;
}
RendererInitInfo;

bool Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_ListenToWindowEvent(SDL_Event event);

typedef union PipelineKey
{
    struct
    {
        // This is a bitfield if you haven't seen this syntax before, it's pretty cool!
        uint64_t pipeline_type : 2;  // Graphics or Compute for now, maybe Raytracing as well in future.
        uint64_t shader_id     : 16; // Index into a shader array (NPR, PBR, Outline)
        uint64_t pass_id       : 8;  // To match against PassIDs collected from BuildFrameGraph

        // Graphics Pipeline Bits
        uint64_t vertex_type   : 4;  // Static, Skinned, possibly Morph (for cloth) etc.
        uint64_t depth_test    : 1;  // On/Off
        uint64_t depth_write   : 1;  // On/Off
        uint64_t depth_op      : 3;  // Less, Equal, Always (for X-ray if we want that)
        uint64_t stencil_mode  : 4;  // None, Write, Test
        uint64_t cull_mode     : 2;  // None, Front, Back
        uint64_t blend_mode    : 4;  // Opaque, Alpha, Additive
        // ... remaining bits for future use
    };

    uint64_t value;
}
PipelineKey;

// Recreated at frame beginning (allows a dynamic render graph)
typedef struct PassIDs
{
    // Metadata: Needed because double/triple/etc. buffered swapchain
    uint32_t swapchain_image_index;
    uint32_t swapchain_image_resource_id;

    uint32_t swapchain_pass;  // Outputs to current swapchain image id.
}
PassIDs;

uint32_t Renderer_BeginFrame();
PassIDs Renderer_BuildFrameGraph(uint32_t swapchain_image_index);
void Renderer_Draw(uint32_t entity_id, PipelineKey pipeline_key);
void Renderer_EndFrame(PassIDs pass_ids);

#endif  // ENGINE_RENDERER_H
