#ifndef SHADERSRC_SHARED_BUFFERS_GLSL
#define SHADERSRC_SHARED_BUFFERS_GLSL

/*  NOTE: Scalar block layout,
    but for performance I may throw some padding in anyway :)
*/

struct SceneData
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 cam_position;
    float time;
    float near_plane;
    float far_plane;
    float aspect;
    uvec2 rendertarget_size;
};

struct ObjectData
{
    mat4 model;
};

struct MaterialData
{
    // TODO: Change this to a standard glTF pbr material instead of this shit
    vec4  base_color;  // Alpha stored in base_color.a
    float metalness;
    float roughness;
    vec3  emissive_factor;
    uint32_t blend_mode;
    float alpha_cutoff;

    uint32_t sampler_idx;
    
    uint32_t texture_idx_basecolor;
    uint32_t texture_idx_emissive;
    // Full PBR would be something like the following, but dunno how much we want that at the moment.:
    // uint32_t texture_idx_basecolor_rgb_metalness_a;
    // uint32_t texture_idx_emissive_rgb_roughness_a;
    // uint32_t texture_idx_normalmap;
};

#define MAX_POINTLIGHTS 500
#define MAX_SPOTLIGHTS  500
struct PointLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
};

struct SpotLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
    vec4 direction_and_cone_cutoff;
    // TODO: For future shadow map cache, add a dirty bit for if it has moved
};

#ifndef IS_GLSL
    
    typedef struct PushConstant_DrawCall PushConstant_DrawCall;
    typedef struct SceneData             SceneData;
    typedef struct ObjectData            ObjectData;
    typedef struct Vertex                Vertex;
    typedef struct MaterialData          MaterialData;
    typedef struct PointLight            PointLight;
    typedef struct SpotLight             SpotLight;

#else

    // Pointer types for global buffers
    layout (buffer_reference, scalar) readonly buffer SceneBuffer
    {
        SceneData scene;
    };
    layout (buffer_reference, scalar) readonly buffer ObjectBuffer
    {
        ObjectData object;
    };
    layout (buffer_reference, scalar) readonly buffer MaterialBuffer
    {
        MaterialData materials[];
    };
    layout (buffer_reference, scalar) readonly buffer LightsBuffer
    {
        PointLight pointlights[];
        SpotLight  spotlights[];
    };

    // Pointer types for current mesh:
    layout (buffer_reference, scalar) readonly buffer IndexBuffer
    {
        uint indices[];
    };
    layout (buffer_reference, scalar) readonly buffer JointBuffer
    {
        mat4 joints[];
    };
    // Vertex attributes seperated into their own buffers:
    layout (buffer_reference, scalar) readonly buffer VPositionBuffer     { vec3  positions[]; };
    layout (buffer_reference, scalar) readonly buffer VTexcoordBuffer     { vec2  texcoords[]; };
    layout (buffer_reference, scalar) readonly buffer VNormalBuffer       { vec3  normals[];   };
    layout (buffer_reference, scalar) readonly buffer VColorBuffer        { vec3  colors[];    };
    layout (buffer_reference, scalar) readonly buffer VJointIDsBuffer     { uvec4 joint_ids[]; };
    layout (buffer_reference, scalar) readonly buffer VJointWeightsBuffer { vec4  weights[];   };

#endif  // IS_GLSL

#endif  // SHADERSRC_SHARED_BUFFERS_GLSL
