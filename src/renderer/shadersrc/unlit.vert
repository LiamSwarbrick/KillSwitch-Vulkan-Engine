#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require
#include "shared_constants.glsl"

struct Vertex
{
    vec3 pos;
    vec2 uv;
    vec3 normal;
    vec4 color;
    uvec4 joint_ids;  // Future: For skinning
    vec4 weights;     // Future: For skinning
};

layout(buffer_reference, scalar) readonly buffer GlobalBuffer
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
};
layout(buffer_reference, scalar) readonly buffer ObjectBuffer { mat4 model; };
layout(buffer_reference, scalar) readonly buffer VertexBuffer { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer JointBuffer { mat4 joints[]; };

layout(push_constant, scalar) uniform PushConstants
{
    uint64_t global_ptr;
    uint64_t object_ptr;
    uint64_t vertex_ptr;
    uint64_t joint_ptr;
    uint64_t material_ptr;
    uint32_t material_idx;
} pc;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main()
{
    // Cast pointers
    GlobalBuffer scene = GlobalBuffer(pc.global_ptr);
    ObjectBuffer obj   = ObjectBuffer(pc.object_ptr);
    VertexBuffer vb    = VertexBuffer(pc.vertex_ptr);
    
    Vertex v = vb.vertices[gl_VertexIndex];
    mat4 model_matrix = obj.model;

    // Optional Skinning Logic
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
