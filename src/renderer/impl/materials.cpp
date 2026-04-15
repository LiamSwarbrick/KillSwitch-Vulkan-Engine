#include "materials.h"
#include "shaders.h"

// NOTE: See render_types.h's MATERIAL_LIST for the list of materials
//       This file is where each material's shaders and metadata are defined.
// Remember if tertiary or more shader's are needed, update drawcall.cpp::AddDrawCall
extern const MaterialConfigs g_material_configs = {
    .by_name = {
        .MAT_UNLIT_OPAQUE = {
            .primary_shader_id = SHADER_UNLIT,
            .secondary_shader_id = SHADER_NONE,
            .is_opaque = 1,
            .is_ui = 0
        },

        .MAT_LIT_OPAQUE = {
            .primary_shader_id = SHADER_LIT,
            .secondary_shader_id = SHADER_NONE,
            .is_opaque = 1,
            .is_ui = 0
        }
    }
};

#warning Remember to use MaterialConfigs::is_opaque/is_ui stuff
