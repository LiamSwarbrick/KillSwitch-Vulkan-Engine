#ifndef RENDERER_PIPELINE_HASHING_H
#define RENDERER_PIPELINE_HASHING_H

#include "vulkan_wrapper.h"

typedef union PipelineKey
{
    struct
    {
        // This is a bitfield if you haven't seen this syntax before, it's pretty cool!
        uint64_t pipeline_type : 2;  // Graphics or Compute for now, maybe Raytracing as well in future.
        uint64_t shader_id     : 16; // Index into a shader array (NPR, PBR, Outline)

        // Graphics Pipeline Bits
        uint64_t vertex_type   : 4;  // Static, Skinned, possibly Morph (for cloth) etc.
        uint64_t depth_test    : 1;  // On/Off
        uint64_t depth_write   : 1;  // On/Off
        uint64_t depth_op      : 3;  // Less, Equal, Always (for X-ray if we want that)
        uint64_t stencil_mode  : 4;  // None, Write, Test
        uint64_t cull_mode     : 2;  // None, Front, Back
        uint64_t blend_mode    : 4;  // Opaque, Alpha, Additive
        uint64_t pass_id       : 8;  // To match against RenderTarget formats
        // ... remaining bits for future use
    };

    uint64_t value;
}
PipelineKey;

typedef struct PipelineEntry
{
    uint64_t key;
    VkPipeline value;
}
PipelineEntry;

void       PK_Init(PipelineEntry** pipeline_map_ref);
VkPipeline PK_GetOrCreatePipeline(PipelineEntry** pipeline_map_ref, PipelineKey key);
void       PK_Shutdown(PipelineEntry** pipeline_map_ref, VkDevice device);

typedef enum
{
    PK_PIPELINE_TYPE_COMPUTE,
    PK_PIPELINE_TYPE_GRAPHICS,

    NUM_PK_PIPELINE_TYPES
}
PK_PipelineType;

#endif  // RENDERER_PIPELINE_HASHING_H
