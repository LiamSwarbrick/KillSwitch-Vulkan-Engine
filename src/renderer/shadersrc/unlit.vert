#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"

layout(push_constant, scalar) uniform PushConstants
{
    GraphicsPushConstants pc;
};

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_color;

void main()
{
    // Cast pointers
    SceneData scene  = SceneBuffer(pc.scene_ptr).scene;
    ObjectData obj   = ObjectBuffer(pc.object_ptr).object;
    
    // Pull vertex data
    IndexBuffer ib   = IndexBuffer(pc.index_ptr);
    uint index = ib.indices[gl_VertexIndex];

    vec3 v_pos    = VPositionBuffer(pc.v_positions_ptr).positions[index];
    vec2 v_uv     = VTexcoordBuffer(pc.v_texcoords_ptr).texcoords[index];
    vec3 v_normal = VNormalBuffer(pc.v_normals_ptr).normals[index];
    vec3 v_color  = vec3(1.0);
    if (pc.v_colors_ptr != 0)
        v_color  = VColorBuffer(pc.v_colors_ptr).colors[index];
    
    mat4 model_matrix = obj.model;

    // Optional Skinning Logic (shader specialization constants btw)
    if (CURRENT_VERTEX_TYPE == VERTEX_TYPE_SKINNED)
    {
        // Pull skinned vertex data
        uvec4 v_joint_ids = VJointIDsBuffer(pc.v_joint_ids_ptr).joint_ids[index];
        vec4 v_weights    = VJointWeightsBuffer(pc.v_joint_weights_ptr).weights[index];

        JointBuffer jb = JointBuffer(pc.joint_ptr);
        mat4 skin = 
            jb.joints[v_joint_ids.x] * v_weights.x +
            jb.joints[v_joint_ids.y] * v_weights.y +
            jb.joints[v_joint_ids.z] * v_weights.z +
            jb.joints[v_joint_ids.w] * v_weights.w;
        
        model_matrix = model_matrix * skin;
    }

    out_uv = v_uv;
    out_color = v_color;
    gl_Position = scene.view_proj * model_matrix * vec4(v_pos, 1.0);
}
