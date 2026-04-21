#include "materials.h"
#include "shaders.h"

// NOTE: See render_types.h's MATERIAL_LIST for the list of materials
//       This file is where each material's shaders and metadata are defined.
// Remember if tertiary or more shader's are needed, update drawcall.cpp::AddDrawCall
extern const MaterialConfigs g_material_configs = {
    .by_name = {
        .MAT_UNLIT = {
            .primary_shader_id = SHADER_UNLIT,
            .secondary_shader_id = SHADER_NONE,
            .is_ui = 0
        },

        .MAT_LIT = {
            .primary_shader_id = SHADER_LIT,
            .secondary_shader_id = SHADER_NONE,
            .is_ui = 0
        },

        .MAT_LIT_OUTLINE = {
            .primary_shader_id = SHADER_LIT,
            .secondary_shader_id = SHADER_OUTLINE,
            .is_ui = 0
        },
    }
};

#warning Remember to use MaterialConfigs::is_ui stuff eventually maybe if it's useful for determining ui renderables
