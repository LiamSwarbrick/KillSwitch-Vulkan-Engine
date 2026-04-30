#ifndef SHADERSRC_SHARED_VERTEX_FETCH_GLSL
#define SHADERSRC_SHARED_VERTEX_FETCH_GLSL

#include "shared_buffers.glsl"
#include "shared_push_constants.glsl"

void fetch_vertex(
    uint index,             // <- Index buffer index, which is gl_VertexIndex because we are submitting vkCmdDraw per index instead
    out uint vertex_buf_index,
    out vec3 v_pos,
    out vec2 v_uv,
    out vec3 v_normal,
    out vec3 v_color)
{
    // Pull vertex data
    IndexBuffer ib   = IndexBuffer(push.dc.index_ptr);
    vertex_buf_index = ib.indices[index];

    v_pos    = VPositionBuffer(push.dc.v_positions_ptr).positions[vertex_buf_index];
    v_uv     = VTexcoordBuffer(push.dc.v_texcoords_ptr).texcoords[vertex_buf_index];
    v_normal = VNormalBuffer(push.dc.v_normals_ptr).normals[vertex_buf_index];
    v_color  = vec3(1.0);
    if (push.dc.v_colors_ptr != 0)
        v_color  = VColorBuffer(push.dc.v_colors_ptr).colors[vertex_buf_index];
}

void fetch_vertex_pos_uv(
    uint index,
    out uint vertex_buf_index,
    out vec3 v_pos,
    out vec2 v_uv
)
{
    // Pull only vertex position (for depth passes)
    IndexBuffer ib   = IndexBuffer(push.dc.index_ptr);
    vertex_buf_index = ib.indices[index];

    v_pos    = VPositionBuffer(push.dc.v_positions_ptr).positions[vertex_buf_index];    
    v_uv     = VTexcoordBuffer(push.dc.v_texcoords_ptr).texcoords[vertex_buf_index];
}

void fetch_vertex_pos_normal(
    uint index,             // <- Index buffer index, which is gl_VertexIndex because we are submitting vkCmdDraw per index instead
    out uint vertex_buf_index,
    out vec3 v_pos,
    out vec3 v_normal)
{
    // Pull vertex data
    IndexBuffer ib   = IndexBuffer(push.dc.index_ptr);
    vertex_buf_index = ib.indices[index];

    v_pos    = VPositionBuffer(push.dc.v_positions_ptr).positions[vertex_buf_index];
    v_normal = VNormalBuffer(push.dc.v_normals_ptr).normals[vertex_buf_index];
}


mat4 compute_model_matrix(uint vertex_buf_index)
{
    ObjectData obj   = ObjectBuffer(push.dc.object_ptr).object;
    mat4 model_matrix = obj.model;

    // Optional Skinning Logic (shader specialization constants btw)
    if (CURRENT_VERTEX_TYPE == VERTEX_TYPE_SKINNED)
    {
        // Pull skinned vertex data
        uvec4 v_joint_ids = VJointIDsBuffer(push.dc.v_joint_ids_ptr).joint_ids[vertex_buf_index];
        vec4 v_weights    = VJointWeightsBuffer(push.dc.v_joint_weights_ptr).weights[vertex_buf_index];

        JointBuffer jb = JointBuffer(push.dc.joints_ptr);
        mat4 skin = 
            jb.joints[v_joint_ids.x] * v_weights.x +
            jb.joints[v_joint_ids.y] * v_weights.y +
            jb.joints[v_joint_ids.z] * v_weights.z +
            jb.joints[v_joint_ids.w] * v_weights.w;
        
        model_matrix = model_matrix * skin;
    }

    return model_matrix;
}

#endif  // SHADERSRC_SHARED_VERTEX_FETCH_GLSL
