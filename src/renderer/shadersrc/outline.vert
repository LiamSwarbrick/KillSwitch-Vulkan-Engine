#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"
#include "common/shared_vertex_fetch.glsl"

invariant gl_Position;

// NOTE: This shader relies on front face culling
void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;
    
    uint vertex_buf_index;
    vec3 v_pos;
    vec3 v_normal;

    fetch_vertex_pos_normal(gl_VertexIndex, vertex_buf_index, v_pos, v_normal);
    mat4 model_matrix = compute_model_matrix(vertex_buf_index);

    // Move vertex in direction of normal by a tiny fixed amount
    // NOTE: This isn't a scale factor, because we want the outline to be object independent
    v_pos += v_normal * 0.007;
    vec4 hull_pos = scene.view_proj * model_matrix * vec4(v_pos, 1.0);

    gl_Position = hull_pos;
}
