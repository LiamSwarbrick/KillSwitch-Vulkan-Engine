#ifndef RENDERER_SHADERS_H
#define RENDERER_SHADERS_H

#include "vulkan_wrapper.h"
#include "pipeline_keying.h"

// Shader Registry
//

#define SHADER_LIST \
    X(SHADER_UNLIT)

typedef enum
{
    #define X(name) name,
    SHADER_LIST
    #undef X

    SHADER_COUNT,
    SHADER_NONE
}
ShaderID;

typedef struct PipelineShaderSet
{
    PK_PipelineType pipeline_type;
    union
    {
        struct
        {
            VkShaderModuleCreateInfo compute_shader;
        } compute;

        struct
        {
            VkShaderModuleCreateInfo vertex_shader;
            VkShaderModuleCreateInfo fragment_shader;
        } graphics;
    };
}
PipelineShaderSet;

// Used when pipeline hashing has to create a new pipeline.
// shader_id is part of PipelineKey and indexes into this array
typedef struct ShaderRegistry
{
    PipelineShaderSet shaders[SHADER_COUNT];
}
ShaderRegistry;

void ShaderRegistry_Init();
void ShaderRegistry_Shutdown();

#endif  // RENDERER_SHADERS_H
