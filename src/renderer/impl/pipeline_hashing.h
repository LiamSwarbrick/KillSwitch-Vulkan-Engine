#ifndef RENDERER_PIPELINE_HASHING_H
#define RENDERER_PIPELINE_HASHING_H

#include "../renderer.h"
#include "vulkan_wrapper.h"

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

// Shader Registry

typedef union PipelineShaders
{
    VkShaderModuleCreateInfo compute_shader;
    struct
    {
        VkShaderModuleCreateInfo vertex_shader;
        VkShaderModuleCreateInfo fragment_shader;
    };
}
PipelineShaders;

#endif  // RENDERER_PIPELINE_HASHING_H
