#ifndef RENDERER_SHADERS_H
#define RENDERER_SHADERS_H

#include "vulkan_wrapper.h"
#include "pipeline_keying.h"

#include "glm/glm.hpp"

typedef struct PushConstants
{
    uint64_t global_ptr;    // Scene data (View/Proj)
    uint64_t object_ptr;    // Per-instance data (Model matrix)
    uint64_t vertex_ptr;    // Vertex attributes
    uint64_t joint_ptr;     // Skinning matrices (0 if static)
    uint32_t material_idx;  // Index into material SSBO
}
PushConstants;

typedef struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec4 color;
    glm::uvec4 joint_ids;  // Future: For skinning
    glm::vec4 weights;     // Future: For skinning
}
Vertex;


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
