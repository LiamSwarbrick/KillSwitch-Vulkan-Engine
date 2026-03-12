#include "materials.h"
#include "shaders.h"

extern const MaterialConfigs g_material_configs = {
    .by_name = {
        .MAT_UNLIT = {
            .primary_shader_id = SHADER_UNLIT,
            .secondary_shader_id = SHADER_NONE,
            .is_opaque = 1,
            .is_ui = 0
        }
    }
};
