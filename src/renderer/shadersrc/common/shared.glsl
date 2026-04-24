#ifndef SHADERSRC_SHARED_GLSL
#define SHADERSRC_SHARED_GLSL

#ifdef IS_GLSL

    #extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
    #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

    #extension GL_EXT_scalar_block_layout : require
    #extension GL_EXT_buffer_reference : require

    // Required to index the partially bound descriptor arrays below with a dynamic variable
    #extension GL_EXT_nonuniform_qualifier : require

    // Bindless heap for textures and samplers is the only use of descriptor sets here
    layout (set = 0, binding = 0) uniform texture2D global_textures[];
    layout (set = 0, binding = 1) uniform sampler   global_samplers[];

#endif


#include "shared_types.glsl"
#include "shared_constants.glsl"
#include "shared_buffers.glsl"
#include "shared_push_constants.glsl"
#include "shared_lighting.glsl"
#include "sampler_indices.glsl"

// Include vertex fetch for vertex shaders and material read for frag shader
// #ifdef IS_GLSL

//     #include "shared_vertex_fetch.glsl"
//     #include "shared_material_read.glsl"

// #endif

#endif  // SHADERSRC_SHARED_GLSL
