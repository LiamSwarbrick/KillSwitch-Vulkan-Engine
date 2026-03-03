#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"

// Recreated at each swapchain resize.
typedef struct ResourcesIDs
{
    b32 resources_created;

    // Resource IDs:
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];
}
ResourcesIDs;

void CreateResources();
void DestroyResources();


// Future
// // Recreated only when swapchain format changes
// // In that case, the pipeline hash should just empty all of them.
// // Then they will be rebuilt lazily.
// typedef struct PipelineIDs
// {

// }
// PipelineIDs;

#endif  // RENDERER_PASS_DEFINITIONS_H
