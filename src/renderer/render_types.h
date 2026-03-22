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
    uint32_t     vertex_type;  // Static or skinned (FUTURE: morph target?)
    MaterialType mat_type;     // Selects which shader to use (or multiple shaders if it's a multipass material type)
    float        sort_depth;   // <-TODO unused.

    uint32_t vertex_rid;    // Buffer of vertices  (TODO: Switch to structure of arrays, to avoid unused attributes i.e. positions_rid, uvs_rid, normals_rids, joints_rid, etc...)
    uint32_t index_rid;     // Buffer of vertex indices
    uint32_t material_id;   // Index into the global Material SSBO
    uint64_t object_ptr;    // GPU Address of the ObjectData (e.g. model matrix)
    uint64_t joint_ptr;     // GPU Address of Joint matrices (0 if static)
}
Renderable;

typedef struct RenderView
{
    Renderable* items;  // stb_ds array
}
RenderView;

#endif  // RENDERER_RENDER_TYPES_H
