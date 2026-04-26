#ifndef RENDERER_RENDER_TYPES_H
#define RENDERER_RENDER_TYPES_H

// Way for game to communicate thing sfor the renderer
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "shadersrc/common/shared.glsl"

#define MAX_RENDERED_OBJECTS 10000

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

// WARNING(Liam): Make sure core/imported_components.h is up to date with this
#define MATERIAL_LIST(X) \
    X(MAT_UNLIT)         \
    X(MAT_LIT)           \
    X(MAT_LIT_OUTLINE)


typedef enum
{
    #define X(name) name,
    MATERIAL_LIST(X)
    #undef X
    MATERIAL_TYPE_COUNT
}
MaterialType;

typedef struct PrimitiveRIDs
{
    uint32_t index_buf_rid;
    uint32_t material_index;  // Index into the global Material SSBO

    // Set to UINT32_MAX for unused attribute (specifically cuz static meshes don't have joints and color is optional too)
    uint32_t v_pos_buf_rid;
    uint32_t v_texcoord_buf_rid;
    uint32_t v_normal_buf_rid;
    uint32_t v_color_buf_rid;
    uint32_t v_joint_ids_buf_rid;
    uint32_t v_joint_weights_buf_rid;    
}
PrimitiveRIDs;

#define MAX_PRIMITIVES 32
typedef struct MeshRIDs
{
    uint32_t primitive_count;
    PrimitiveRIDs primitives[MAX_PRIMITIVES];
}
MeshRIDs;

typedef struct MeshPrefab
{
    VertexType vertex_type;  // Static or skinned (FUTURE: morph target?)
    MaterialType mat_type;   // Selects which shader to use (or multiple shaders if it's a multipass material type e.g. MAT_PBR_WITH_OUTLINE)
    MeshRIDs mesh_rids;
}
MeshPrefab;

typedef struct Renderable
{
    glm::mat4 transform;
    MeshPrefab mesh_prefab;  // The GPU resource buffers containing the vertex and index data
    
    // CPU-side joints buffer we memcpy from to GPU joints buffer
    uint32_t joint_count;
    glm::mat4* joints;    // <- Pointer to animation system side joints array
    // NOTE: Fucking make sure joints arrays are not allocated every frame
}
Renderable;

#endif  // RENDERER_RENDER_TYPES_H
