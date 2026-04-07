#ifndef SHADERSRC_SAMPLER_INDICES_GLSL
#define SHADERSRC_SAMPLER_INDICES_GLSL

#ifdef __cplusplus
// TODO: Add more address modes than just REPEAT
//       Also support for LUT textures (look up tables) e.g. for LTC area lights
typedef enum
{
    FG_SAMPLER_NEAREST_REPEAT      =0,
    FG_SAMPLER_LINEAR_REPEAT       =1,
    FG_SAMPLER_ANISOTROPIC_REPEAT  =2,
    FG_SAMPLER_SHADOW              =3,

    FG_SAMPLER_COUNT,
    FG_SAMPLER_NOT_SAMPLABLE,  // For output resources
}
FG_SamplerType;
#endif

#ifndef __cplusplus
    #define FG_SAMPLER_NEAREST_REPEAT     0
    #define FG_SAMPLER_LINEAR_REPEAT      1
    #define FG_SAMPLER_ANISOTROPIC_REPEAT 2
    #define FG_SAMPLER_SHADOW             3
#endif

#endif  // SHADERSRC_SAMPLER_INDICES_GLSL
