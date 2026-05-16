#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"
#include "renderer/render_types.h"
#include "mapped_linear_allocator.h"

#define MAX_SCENE_MESHES    2000
#define MAX_SCENE_MATERIALS 2000

typedef struct RingBufferedRIDs
{
    // Global (on startup)
    uint32_t scenes_buffer_rid;
    uint32_t objects_buffer_rid;
    uint32_t joints_buffer_rid;

    // Arenas that reset each frame
    MappedArena scenes_arena;
    MappedArena object_transforms;
    MappedArena joint_transforms;
    
    uint32_t lights_header_buffer_rid;
    uint32_t point_lights_buffer_rid;
    uint32_t spot_lights_buffer_rid;
    uint32_t spotlight_shadowmap_index_buffer_rid;
    uint32_t shadowmap_spotlight_camera_buffer_rid;
    uint32_t point_light_indices_buffer_rid;
    uint32_t spot_light_indices_buffer_rid;
    uint32_t cluster_offsets_buffer_rid;
}
RingBufferedRIDs;

// Recreated at each swapchain resize.
typedef struct ResourceIDs
{
    b32 startup_resources_created;
    b32 window_resources_created;

    // Global (on startup)
    uint32_t materials_buffer_rid;

    // Window Dependent
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];
    uint32_t depth_buffer_rid;
    uint32_t forward_target_rid;
    uint32_t hdr_color_target_rid;
    uint32_t ldr_color_target_rid;

    uint32_t shadow_map_rids[MAX_SHADOWMAPS];
    // FUTURE: Atlassed maps with the scissor rectangle for better usage.
    // I.e. thin spotlights can use a smaller sized image, but it's too expensive to allocate at run time
    //      so atassing the shadow (e.g. splitting up a massive texture into tiles) is a better solution

    // Ring buffered resources
    RingBufferedRIDs ring[MAX_SWAPCHAIN_IMAGE_COUNT];
}
ResourceIDs;

void CreateOrRecreateResources(FG_ResourceFlags types_to_create);
void DestroyResources();

#endif  // RENDERER_PASS_DEFINITIONS_H
