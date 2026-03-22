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
layout(location = 1) out vec4 out_color;

void main()
{
    // Cast pointers
    SceneData scene  = SceneBuffer(pc.scene_ptr).scene;
    ObjectData obj   = ObjectBuffer(pc.object_ptr).object;
    VertexBuffer vb  = VertexBuffer(pc.vertex_ptr);
    IndexBuffer ib   = IndexBuffer(pc.index_ptr);
    
    uint index = ib.indices[gl_VertexIndex];
    Vertex v = vb.vertices[index];
    mat4 model_matrix = obj.model;

    // Optional Skinning Logic (shader specialization constants, btw)
    if (CURRENT_VERTEX_TYPE == VERTEX_TYPE_SKINNED)
    {
        JointBuffer jb = JointBuffer(pc.joint_ptr);
        mat4 skin = 
            jb.joints[v.joint_ids.x] * v.weights.x +
            jb.joints[v.joint_ids.y] * v.weights.y +
            jb.joints[v.joint_ids.z] * v.weights.z +
            jb.joints[v.joint_ids.w] * v.weights.w;
        
        model_matrix = model_matrix * skin;
    }

    out_uv = v.uv;
    out_color = v.color;
    gl_Position = scene.view_proj * model_matrix * vec4(v.pos, 1.0);
}
