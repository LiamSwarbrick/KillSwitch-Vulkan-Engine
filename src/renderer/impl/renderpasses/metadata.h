#ifndef RENDERER_RENDERPASSES_METADATA_PASS_H
#define RENDERER_RENDERPASSES_METADATA_PASS_H

#include "../framegraph.h"
#include "../pipeline_keying.h"

// TODO: Add a renderview cache (ideally just big preallocated arrays)
// OR, just gather all renderables, but in each pass, skip over the irrelevant ones.

typedef enum
{
    PASS_TYPE_SWAPCHAIN_PASS,
    PASS_TYPE_GBUFFER_WRITE,
    PASS_TYPE_DEFERRED_LIGHTING_UNLIT,
    PASS_TYPE_IMGUI,

    PASS_TYPE_COUNT,
    PASS_TYPE_INVALID
}
PassType;
static_assert(PASS_TYPE_COUNT <= MAX_PASSES, "Must increase MAX_PASSES in framegraph.h to store these.");
static_assert(PASS_TYPE_COUNT < 1 << PKEY_NUM_BITS_PASS_TYPE,
    "More passes than pipeline key can index. Must give more bits by increasing PKEY_NUM_BITS_PASS_TYPE in pipeline_hashing.h."
);

void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data);
void ImGuiPass_Execute(VkCommandBuffer cmd, void* user_data);

#endif  // RENDERER_RENDERPASSES_METADATA_PASS_H
