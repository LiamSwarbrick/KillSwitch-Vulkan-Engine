#ifndef RENDERER_SHADERS_H
#define RENDERER_SHADERS_H

#include "vulkan_wrapper.h"
#include "pipeline_keying.h"

#include "glm/glm.hpp"

typedef struct PushConstants
{
    uint64_t scene_ptr;     // Scene data (View/Proj)
    uint64_t object_ptr;    // Per-instance data (Model matrix)
    uint64_t vertex_ptr;    // Vertex attributes (Pulling)
    uint64_t joint_ptr;     // Skinning matrices (0 if static)
    uint64_t material_ptr;  // Material SSBO address
    uint32_t material_idx;  // Which material in the SSBO
    uint32_t padding;       // Keep 16-byte alignment
}
PushConstants;

typedef struct SceneBufferData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
}
SceneBufferData;

typedef struct ObjectData
{
    glm::mat4 model;
}
ObjectData;

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

// Temp, simpler material
typedef struct MaterialData
{
    glm::vec4 base_color;
    uint32_t texture_idx;  // Index into Bindless Heap
    float alpha_cutoff;
    uint32_t padding[2];
}
MaterialData;

// For object transforms
typedef struct TransientBuffer
{
    uint32_t rid;
    uint64_t gpu_base_address;
    uint8_t* mapped_data;
    uint32_t current_offset;
    uint32_t total_size;
}
TransientBuffer;

// TODO: Unexpose this, and use a PrepareRenderView or something that gets entities from the scene
uint64_t push_object(TransientBuffer* tb, ObjectData object);

TransientBuffer CreateTransientBuffer(uint32_t underlying_resource_id);
uint64_t GetResourceBufferDeviceAddress(uint32_t rid);
void SubmitDraw(VkCommandBuffer cmd, Renderable* r, PipelineKey key);

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

void UpdateGlobalSceneData();
void SubmitDraw(VkCommandBuffer cmd, Renderable* r, PipelineKey key);

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
