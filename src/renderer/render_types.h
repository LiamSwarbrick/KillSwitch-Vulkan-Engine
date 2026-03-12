#ifndef RENDERER_RENDER_TYPES_H
#define RENDERER_RENDER_TYPES_H

// Way for game to communicate thing sfor the renderer
#include "core/core.h"
#include "glm/glm.hpp"

/*
Material Example Ideas:
MAT_UNLIT,             // Skybox
MAT_PBR_LIT,           // Standard PBR/Lit
MAT_TOON,              // NPR
MAT_OUTLINE,           // Special effect
MAT_PBR_WITH_OUTLINE,  // Multipass material example
MAT_PAUSE_UI           // E.g. pause menu material renderables are part of a pass that only show when paused game flag is active
MAT_HUD                // E.g. Minimap, current weapon, ammo. These are ui elements that are on the screen by default.
*/

#define MATERIAL_LIST \
    X(MAT_UNLIT)

typedef enum
{
    #define X(name) name,
    MATERIAL_LIST
    #undef X
    MATERIAL_TYPE_COUNT
}
MaterialType;

typedef struct Renderable
{
    uint32_t mesh_rid;      // RID from the registry
    uint32_t material_id;   // Index into bindless SSBO for colors/textures
    MaterialType type;
    glm::mat4 transform;    // Model matrix.
    // NOTE: If it's a UI material then we can infer an ortho or identity projection,
    // else it's the camera projection that gets applied on top of it. Aka, the camera matrix is dependent on the renderpass, not the renderable.
}
Renderable;

typedef struct RenderView
{
    Renderable* items;  // stb_ds array
}
RenderView;

#endif  // RENDERER_RENDER_TYPES_H
