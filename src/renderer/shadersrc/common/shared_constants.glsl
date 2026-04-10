#ifndef SHADERSRC_SHARED_CONSTANTS_GLSL
#define SHADERSRC_SHARED_CONSTANTS_GLSL

#ifdef IS_GLSL

    // Shader specialization constants
    layout(constant_id = 0) const uint CURRENT_VERTEX_TYPE = 0;
    layout(constant_id = 1) const uint CURRENT_BLEND_MODE  = 0;

    #define UINT32_MAX 0xFFFFFFFF

#endif  // IS_GLSL


#endif  // SHADERSRC_SHARED_CONSTANTS_GLSL
