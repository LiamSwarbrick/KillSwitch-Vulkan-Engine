#ifndef SHADERSRC_SHARED_PUSH_CONSTANTS_GLSL
#define SHADERSRC_SHARED_PUSH_CONSTANTS_GLSL

struct PushConstant_DrawCall
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

struct PushConstant_PassHeader
{
    uint32_t texture_indices[16];
};

struct FullPushConstants_Graphics  // Defined for the CPU side to use
{
    PushConstant_DrawCall dc;
    PushConstant_PassHeader pass;
};

#ifdef IS_GLSL

    layout(push_constant, scalar) uniform PushConstants
    {
        FullPushConstants_Graphics push;
    };

#endif  // IS_GLSL

#endif  // SHADERSRC_SHARED_PUSH_CONSTANTS_GLSL