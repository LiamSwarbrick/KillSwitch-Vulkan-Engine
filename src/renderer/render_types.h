#ifndef RENDERER_RENDER_TYPES_H
#define RENDERER_RENDER_TYPES_H

// Way for game to communicate thing sfor the renderer
#include "core/core.h"
#include "glm/glm.hpp"

typedef enum MaterialType
{
    MAT_DEFAULT,      // Standard PBR/Lit
    // MAT_UNLIT,        // UI or Glow
    // MAT_TOON,         // NPR
    // MAT_OUTLINE,      // Special effect
    // MAT_PAUSE_UI      // E.g. pause menu material renderables are part of a pass that only show when paused game flag is active
    // MAT_HUD           // e.g. Minimap, current weapon, ammo.
    NUM_MATERIAL_TYPES
}
MaterialType;

typedef struct Renderable
{
    uint32_t mesh_rid;      // RID from the registry
    uint32_t material_id;   // Index into bindless SSBO for colors/textures
    MaterialType type;      // The Game's "Intent"
    glm::mat4 transform;
}
Renderable;

typedef struct RenderView
{
    Renderable* items;  // stb_ds array
}
RenderView;

#endif  // RENDERER_RENDER_TYPES_H
