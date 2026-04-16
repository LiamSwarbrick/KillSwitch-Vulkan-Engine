#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"
#include "renderer/render_types.h"

#define MAX_SCENE_MESHES    2000
#define MAX_SCENE_MATERIALS 2000

// Recreated at each swapchain resize.
typedef struct ResourceIDs
{
    b32 startup_resources_created;
    b32 window_resources_created;

    // Global (on startup)
    uint32_t global_scene_buffer_rid;
    uint32_t objects_buffer_rid;
    uint32_t joints_buffer_rid;
    uint32_t material_ssbo_rid;

    // Window Dependent
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];
    uint32_t depth_buffer_rid;
    uint32_t forward_target_rid;
    uint32_t hdr_color_target_rid;

    // Scene Dependant (NOTE: These are kept track by the ECS/scene, so possibly I don't actually need these)
    // uint32_t num_scene_meshes;
    // MeshPrefab   scene_meshes[MAX_SCENE_MESHES];
    // uint32_t num_scene_materials;
    // MaterialData scene_materials[MAX_SCENE_MATERIALS];


    // Dummy stuff TODO REMOVE
    MeshPrefab dummy_mesh;
    MeshPrefab temp_test_mesh;
    MaterialData temp_test_mat;
}
ResourceIDs;

void CreateOrRecreateResources(FG_ResourceFlags types_to_create);
void DestroyResources();

#endif  // RENDERER_PASS_DEFINITIONS_H
