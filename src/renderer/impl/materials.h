#ifndef RENDERER_MATERIALS_H
#define RENDERER_MATERIALS_H

#include "../render_types.h"  // This defines MATERIAL_LIST
#include "core/my_c_runtime.h"

typedef struct MaterialPipelineInfo
{
    uint16_t primary_shader_id;    // E.g. PBR, TOON, etc..
    uint16_t secondary_shader_id;  // Multi-pass, e.g. OUTLINE.
    // FUTURE: Add tertiary shader etc.. if needed. Set to SHADER_NONE when not used.

    b32 is_ui;
}
MaterialPipelineInfo;

typedef union MaterialConfigs
{
    MaterialPipelineInfo array[MATERIAL_TYPE_COUNT];
    struct
    {
        #define X(name) MaterialPipelineInfo name;
        MATERIAL_LIST(X)  // See render_types.h
        #undef X
    } by_name;  // Required because stupid C++ doesn't have C99 array designators for const initialization. (This trick feels quite clean though to be fair)
}
MaterialConfigs;
extern const MaterialConfigs g_material_configs;

#endif  // RENDERER_MATERIALS_H
