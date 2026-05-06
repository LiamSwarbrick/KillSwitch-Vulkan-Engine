#ifndef SHADERSRC_SHARED_CONSTANTS_GLSL
#define SHADERSRC_SHARED_CONSTANTS_GLSL

#define DEBUG_RENDERMODE_OFF 0
#define DEBUG_RENDERMODE_CLUSTERED_SHADING_HEATMAP 1
#define DEBUG_RENDERMODE_CLUSTERED_SHADING_CLUSTERS 2

#ifdef IS_GLSL

    // Shader specialization constants
    layout (constant_id = 0) const uint CURRENT_VERTEX_TYPE = 0;
    layout (constant_id = 1) const uint CURRENT_BLEND_MODE  = 0;
    layout (constant_id = 2) const uint MSAA_SAMPLE_COUNT   = 1;
    layout (constant_id = 3) const uint DEBUG_RENDERMODE    = 0;

    #define UINT32_MAX 0xFFFFFFFF
    #define PI 3.14159265358979323846

#else

    typedef struct SpecializationData
    {
        uint32_t vertex_type;
        uint32_t blend_mode;
        uint32_t msaa_sample_count;
        uint32_t debug_rendermode;
    }
    SpecializationData;

#endif  // IS_GLSL


#endif  // SHADERSRC_SHARED_CONSTANTS_GLSL
