#version 460
#extension GL_GOOGLE_include_directive : require
#include "common/shared.glsl"

invariant gl_Position;

void main()
{
    SceneData scene  = SceneBuffer(push.dc.scene_ptr).scene;

    uint vertex_buf_index;    
    vec3 v_pos;
    fetch_vertex_pos(gl_VertexIndex, vertex_buf_index, v_pos);

    mat4 model_matrix = compute_model_matrix(vertex_buf_index);

    gl_Position = scene.view_proj * model_matrix * vec4(v_pos, 1.0);
}
