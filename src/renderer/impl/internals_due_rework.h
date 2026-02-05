#ifndef RENDERER_INTERNALS_DUE_REWORK_H
#define RENDERER_INTERNALS_DUE_REWORK_H

#include "vulkan_wrapper.h"
#include "core/my_c_runtime.h"

VkCommandBuffer vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool);

typedef struct OldRenderState
{
    // Single-threaded pools
    // One time submission commands are made more efficient by having a premade command pool lying around specifically for it.
    // It is more efficient to reset the pool for the one_time_command instead of allocating and destroying a command buffer each time.
    // So this is done in begin/end_one_time_submit() functions.
    VkCommandPool onetime_command_pool;
    VkCommandBuffer onetime_command;
    VkFence onetime_command_complete_fence;

    // Default sampler
    b32 use_anisotropic_filtering;
    VkSampler default_sampler;
}
OldRenderState;

#endif  // RENDERER_INTERNALS_DUE_REWORK_H
