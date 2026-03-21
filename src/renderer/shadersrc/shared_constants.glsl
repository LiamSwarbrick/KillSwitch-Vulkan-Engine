// TODO: Maybe a macro that represents either C or C++ instead of __cplusplus which is specific
#ifdef __cplusplus
    #include "glm/glm.hpp"
    typedef glm::mat4 mat4;
    typedef glm::vec4 vec4;
    typedef glm::vec3 vec3;
    typedef glm::vec2 vec2;
    typedef glm::uvec4 uvec4;
#endif

#define VERTEX_TYPE_STATIC 0
#define VERTEX_TYPE_SKINNED 1

#define BLEND_MODE_OPAQUE   0
#define BLEND_MODE_MASKED   1
#define BLEND_MODE_BLEND    2
#define BLEND_MODE_ADDITIVE 3

struct GraphicsPushConstants
{
    uint64_t scene_ptr;     // Scene data (View/Proj)
    uint64_t object_ptr;    // Per-instance data (Model matrix)
    uint64_t vertex_ptr;    // Vertex attributes (Pulling)
    uint64_t joint_ptr;     // Skinning matrices (0 if static)
    uint64_t material_ptr;  // Material SSBO address
    uint32_t material_idx;  // Which material in the SSBO
    uint32_t padding;       // Keep 16-byte alignment
};
struct SceneData
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
};
struct ObjectData
{
    mat4 model;
};
struct Vertex
{
    vec3 pos;
    vec2 uv;
    vec3 normal;
    vec4 color;
    uvec4 joint_ids;  // Future: For skinning
    vec4 weights;     // Future: For skinning
};
struct MaterialData
{
    // TODO: Change this to a standard glTF pbr material instead of this shit
    vec4 base_color;
    uint32_t texture_idx;
    uint32_t sampler_idx;
    float alpha_cutoff;
    uint32_t padding[2];
};


#ifdef __cplusplus
    // I want to keep this C compatiable.
    typedef struct GraphicsPushConstants GraphicsPushConstants;
    typedef struct SceneData SceneData;
    typedef struct ObjectData ObjectData;
    typedef struct Vertex Vertex;
    typedef struct MaterialData MaterialData;
#endif

#ifndef __cplusplus  // TODO: Maybe a specifc GLSL macro

    // Shader specialization constants
    layout(constant_id = 0) const uint CURRENT_VERTEX_TYPE = 0;
    layout(constant_id = 1) const uint CURRENT_BLEND_MODE = 0;

    // Pointer types
    layout(buffer_reference, scalar) readonly buffer SceneBuffer
    {
        SceneData scene;
    };
    layout(buffer_reference, scalar) readonly buffer ObjectBuffer
    {
        ObjectData object;
    };
    layout(buffer_reference, scalar) readonly buffer VertexBuffer
    {
        Vertex vertices[];
    };
    layout(buffer_reference, scalar) readonly buffer JointBuffer
    {
        mat4 joints[];
    };
    layout(buffer_reference, scalar) readonly buffer MaterialBuffer
    {
        MaterialData materials[];
    };
#endif

