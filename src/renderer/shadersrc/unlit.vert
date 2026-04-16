#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_vertex_fetch.glsl"

layout (location = 0) out vec2 uv;
layout (location = 1) out vec3 color;

// Multipass materials require different shaders have reproducable vertex positions
invariant gl_Position;

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    // In
    uint vertex_buf_index;
    vec3 v_pos;
    vec2 v_uv;
    vec3 v_normal;
    vec3 v_color;

    fetch_vertex(gl_VertexIndex, vertex_buf_index, v_pos, v_uv, v_normal, v_color);

    // Compute model matrix also fetches joints if available and applies skinning
    mat4 model_matrix = compute_model_matrix(vertex_buf_index);

    // Out
    uv = v_uv;
    color = v_color;
    gl_Position = scene.view_proj * model_matrix * vec4(v_pos, 1.0);
}
