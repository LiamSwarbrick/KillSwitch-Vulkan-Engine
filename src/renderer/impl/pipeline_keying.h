#ifndef RENDERER_PIPELINE_HASHING_H
#define RENDERER_PIPELINE_HASHING_H

#include "vulkan_wrapper.h"
#include "../render_types.h"

#include "renderer/shadersrc/shared_constants.glsl"
static_assert(sizeof(FullPushConstants_Graphics) <= 256 &&
    "Vulkan 1.4 only guaruntees 256 bytes for pushconstants. So keep within that range."
);

typedef enum
{
    PKEY_NUM_BITS_PIPELINE_TYPE =2,   // Graphics or Compute for now, maybe Raytracing as well in future.
    PKEY_NUM_BITS_SHADER_ID     =16,  // Index into a shader array (NPR, PBR, Outline)
    PKEY_NUM_BITS_PASS_TYPE     =8,   // PassType, not the index of a pass in a framegraph (see renderpasses/metadata.h)
}
PipelineKeyNumBits;

typedef enum
{
    PKEY_MULTISAMPLING_1X = 0,
    PKEY_MULTISAMPLING_2X = 1,
    PKEY_MULTISAMPLING_4X = 2,
    PKEY_MULTISAMPLING_8X = 3
}
PipelineKeyMultisamplingBits;
PipelineKeyMultisamplingBits PK_MultisamplingFlag(VkSampleCountFlagBits sample_count);

typedef union PipelineKey
{
    struct
    {
        uint64_t pipeline_type : PKEY_NUM_BITS_PIPELINE_TYPE;
        uint64_t shader_id     : PKEY_NUM_BITS_SHADER_ID;
        uint64_t pass_type     : PKEY_NUM_BITS_PASS_TYPE;
        // Graphics Pipeline Bits
        uint64_t vertex_type   : 4;  // Static, Skinned, possibly Morph (for cloth) etc.
        uint64_t depth_test    : 1;  // On/Off (VkBool32 VkPipelineDepthStencilStateCreateInfo::depthTestEnable)
        uint64_t depth_write   : 1;  // On/Off
        uint64_t depth_op      : 3;  // VkCompareOp
        uint64_t stencil_mode  : 4;  // None, Write, Test. (NOT YET IMPLEMENTED): TODO since VkStencilOpState not implemented/needed yet
        uint64_t cull_mode     : 2;  // VkCullModeFlagBits
        uint64_t blend_mode    : 4;  // BLEND_MODE_OPAQUE / Alpha / Additive (see shadersrc/shared_constants.glsl)
        uint64_t polygon_mode  : 2;  // VkPolygonMode
        uint64_t front_face    : 1;  // VkFrontFace (we'll use CCW but things like mirrored objects would flip winding)
        uint64_t msaa_samples  : 2;  // PipelineKeyMultisamplingBits. 0=1x, 1=2x, 3=4x, 4=8x samples
        // ... remaining bits for future use
    };

    uint64_t value;
}
PipelineKey;
static_assert(sizeof(PipelineKey) == sizeof(uint64_t), "PipelineKey bitfield must not exceed 64 bits aka 8 bytes");

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

    PK_PIPELINE_TYPE_COUNT,
    PK_PIPELINE_TYPE_INVALID
}
PK_PipelineType;

#endif  // RENDERER_PIPELINE_HASHING_H
