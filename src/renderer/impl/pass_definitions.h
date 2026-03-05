#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"

// Recreated at each swapchain resize.
typedef struct ResourceIDs
{
    b32 resources_created;

    // Resource IDs:
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];

    uint32_t test_v_buffer_rid;
}
ResourceIDs;

void CreateResources();
void DestroyResources();

// Recreated at frame beginning (allows a dynamic render graph)
typedef struct PassIDs
{
    b32 passes_created;

    uint32_t swapchain_pass;  // Outputs to current swapchain image id.
}
PassIDs;


// TODO:
// Recreated only when swapchain format changes
// Future note:
// In that case, the pipeline hash should just empty all of them.
// Then they will be rebuilt lazily.
typedef struct PipelineIDs
{
    int a;
}
PipelineIDs;

#endif  // RENDERER_PASS_DEFINITIONS_H
