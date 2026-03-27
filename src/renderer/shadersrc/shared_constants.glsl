#ifndef SHADERSRC_SHARED_CONSTANTS_GLSL
#define SHADERSRC_SHARED_CONSTANTS_GLSL

// TODO: Maybe a macro that represents either C or C++ instead of __cplusplus which is specific
#ifdef __cplusplus
    #include "glm/glm.hpp"
    typedef glm::mat4 mat4;
    typedef glm::vec4 vec4;
    typedef glm::vec3 vec3;
    typedef glm::vec2 vec2;
    typedef glm::uvec4 uvec4;
#endif

#ifndef __cplusplus
    #define VERTEX_TYPE_STATIC 0
    #define VERTEX_TYPE_SKINNED 1
#else
    typedef enum
    {
        VERTEX_TYPE_STATIC = 0,
        VERTEX_TYPE_SKINNED = 1,

        VERETX_TYPE_COUNT
    }
    VertexType;
#endif

#ifndef __cplusplus
    #define BLEND_MODE_OPAQUE   0
    #define BLEND_MODE_MASKED   1
    #define BLEND_MODE_BLEND    2
    #define BLEND_MODE_ADDITIVE 3
#else
    typedef enum
    {
        BLEND_MODE_OPAQUE   = 0,
        BLEND_MODE_MASKED   = 1,
        BLEND_MODE_BLEND    = 2,
        BLEND_MODE_ADDITIVE = 3,

        BLEND_MODE_COUNT
    }
    BlendMode;
#endif

struct GraphicsPushConstants
{
    uint64_t scene_ptr;     // Scene data (View/Proj)
    uint64_t material_ptr;  // Material SSBO address

    // Per mesh
    uint64_t object_ptr;    // Per-instance data (Model matrix)
    uint64_t joints_ptr;     // Skinning matrices (0 if static)

    // Per primitive:
    uint32_t material_idx;  // Which material in the SSBO
    uint32_t _padding;
    uint64_t index_ptr;      // Index buffer (Pulling)
    uint64_t v_positions_ptr;
    uint64_t v_texcoords_ptr;
    uint64_t v_normals_ptr;
    uint64_t v_colors_ptr;
    uint64_t v_joint_ids_ptr;      // Only for skinned meshes
    uint64_t v_joint_weights_ptr;  // Only for skinned meshes
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
struct MaterialData
{
    // TODO: Change this to a standard glTF pbr material instead of this shit
    vec4 base_color;
    uint32_t texture_idx_basecolor;
    
    uint32_t sampler_idx;
    float alpha_cutoff;
    uint32_t padding[1];
};


#ifdef __cplusplus
    static_assert(sizeof(GraphicsPushConstants) <= 128);

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

    // Pointer types for global buffers
    layout(buffer_reference, scalar) readonly buffer SceneBuffer
    {
        SceneData scene;
    };
    layout(buffer_reference, scalar) readonly buffer ObjectBuffer
    {
        ObjectData object;
    };
    layout(buffer_reference, scalar) readonly buffer MaterialBuffer
    {
        MaterialData materials[];
    };

    // layout(buffer_reference, scalar) readonly buffer VertexBuffer
    // {
    //     Vertex vertices[];
    // };

    // Pointer types for current mesh:
    layout(buffer_reference, scalar) readonly buffer IndexBuffer
    {
        uint indices[];
    };
    layout(buffer_reference, scalar) readonly buffer JointBuffer
    {
        mat4 joints[];
    };
    // Vertex attributes seperated into their own buffers:
    layout(buffer_reference, scalar) readonly buffer VPositionBuffer { vec3 positions[]; };
    layout(buffer_reference, scalar) readonly buffer VTexcoordBuffer { vec2 texcoords[]; };
    layout(buffer_reference, scalar) readonly buffer VNormalBuffer   { vec3 normals[]; };
    layout(buffer_reference, scalar) readonly buffer VColorBuffer    { vec3 colors[]; };
    layout(buffer_reference, scalar) readonly buffer VJointIDsBuffer { uvec4 joint_ids[]; };
    layout(buffer_reference, scalar) readonly buffer VJointWeightsBuffer { vec4 weights[]; };

#endif

#endif  // SHADERSRC_SHARED_CONSTANTS_GLSL
