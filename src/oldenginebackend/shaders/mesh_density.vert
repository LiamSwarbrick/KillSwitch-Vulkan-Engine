#version 450
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 v_position;

#define VS_GEOMETRY_UNIFORMS
#include "../engine/shared_glsl_defs.h"

void main()
{
    vec4 world_pos = pcs.model * vec4(v_position, 1.0);
    gl_Position = u_scene.view_projection * world_pos;
}
