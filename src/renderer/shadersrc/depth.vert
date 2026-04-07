#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"

layout(push_constant, scalar) uniform PushConstants
{
    FullPushConstants_Graphics push;
};

invariant gl_Position;

void main()
{
    // No casting push.pass because forward rendering doesn't read anything.
    // TODO: Switch to deferred?

    // Cast pointers
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;
    ObjectData obj   = ObjectBuffer(push.dc.object_ptr).object;
    
    // Pull only vertex positions
    IndexBuffer ib   = IndexBuffer(push.dc.index_ptr);
    uint index = ib.indices[gl_VertexIndex];
    vec3 v_pos    = VPositionBuffer(push.dc.v_positions_ptr).positions[index];
    mat4 model_matrix = obj.model;

    if (CURRENT_VERTEX_TYPE == VERTEX_TYPE_SKINNED)
    {
        // Pull skinned vertex data
        uvec4 v_joint_ids = VJointIDsBuffer(push.dc.v_joint_ids_ptr).joint_ids[index];
        vec4 v_weights    = VJointWeightsBuffer(push.dc.v_joint_weights_ptr).weights[index];

        JointBuffer jb = JointBuffer(push.dc.joints_ptr);
        mat4 skin = 
            jb.joints[v_joint_ids.x] * v_weights.x +
            jb.joints[v_joint_ids.y] * v_weights.y +
            jb.joints[v_joint_ids.z] * v_weights.z +
            jb.joints[v_joint_ids.w] * v_weights.w;
        
        model_matrix = model_matrix * skin;
    }

    gl_Position = scene.view_proj * model_matrix * vec4(v_pos, 1.0);
}
