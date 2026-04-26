#ifndef SHADERSRC_SHARED_TYPES_GLSL
#define SHADERSRC_SHARED_TYPES_GLSL

#ifndef IS_GLSL

    // Glm's padding cannot be trusted for scalar layout btw
    // #include "glm/glm.hpp"
    // typedef glm::mat4 mat4;
    // typedef glm::vec4 vec4;
    // typedef glm::vec3 vec3;
    // typedef glm::vec2 vec2;
    // typedef glm::uvec2 uvec2;
    // typedef glm::uvec4 uvec4;
    typedef float mat4[16];
    typedef float vec4[4];
    typedef float vec3[3];
    typedef float vec2[2];
    typedef uint32_t uvec2[2];
    typedef uint32_t uvec4[4];

    // NOTE: Ensure these enums match the GLSL #defines below them
    typedef enum
    {
        VERTEX_TYPE_STATIC = 0,
        VERTEX_TYPE_SKINNED = 1,

        VERETX_TYPE_COUNT
    }
    VertexType;

    typedef enum
    {
        BLEND_MODE_OPAQUE   = 0,
        BLEND_MODE_MASKED   = 1,
        BLEND_MODE_BLEND    = 2,
        BLEND_MODE_ADDITIVE = 3,

        BLEND_MODE_COUNT
    }
    BlendMode;

#else

    #define VERTEX_TYPE_STATIC 0
    #define VERTEX_TYPE_SKINNED 1

    #define BLEND_MODE_OPAQUE   0
    #define BLEND_MODE_MASKED   1
    #define BLEND_MODE_BLEND    2
    #define BLEND_MODE_ADDITIVE 3
    
#endif  // IS_GLSL



#endif  // SHADERSRC_SHARED_TYPES_GLSL
