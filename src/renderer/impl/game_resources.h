#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"
#include "renderer/render_types.h"

// Recreated at each swapchain resize.
typedef struct ResourceIDs
{
    b32 startup_resources_created;
    b32 window_resources_created;

    // Window Dependent
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];
    
    // Global (on startup)
    uint32_t global_scene_buffer_rid;
    uint32_t objects_buffer_rid;
    uint32_t material_ssbo_rid;
    
    // Dummy stuff
    MeshPrefab dummy_mesh;
    MeshPrefab temp_test_mesh;
}
ResourceIDs;

void CreateOrRecreateResources(FG_ResourceFlags types_to_create);
void DestroyResources();

#endif  // RENDERER_PASS_DEFINITIONS_H
