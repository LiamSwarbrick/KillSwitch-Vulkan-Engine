#ifndef RENDERER_PASS_DEFINITIONS_H
#define RENDERER_PASS_DEFINITIONS_H

#include "framegraph.h"
#include "internal_structs.h"

typedef struct PassDefinitions
{
    b32 is_created;

    // NOTE: Should store renderpass descriptions but not pipelines.
    // Because pipelines don't need to be recreated everytime the swapchain
    // changes size, but renderpass resources do, so the descriptions and buffers
    // need recreating.

    // Resource IDs:
    uint32_t swapchain_image_rids[MAX_SWAPCHAIN_IMAGE_COUNT];
}
PassDefinitions;

void CreatePassDefinitionsAndResources();
void DestroyPassDefinitionsAndResources();

#endif  // RENDERER_PASS_DEFINITIONS_H
