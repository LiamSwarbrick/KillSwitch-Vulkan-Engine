#include "internal_state.h"

VkCommandBuffer
vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo cmd_alloc_info = {};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.pNext = NULL;
    cmd_alloc_info.commandPool = command_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &command_buffer));
    
    return command_buffer;
}

void old_stuff_init(RenderState* renderstate)
{
    // Init pool and command staging buffers use to perform a one time copy command in a more efficent way than creating a whole command pool each time
    {
        // Transfer pool:
        VkCommandPoolCreateInfo onetime_cmdpool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            // NOTE: The transient flag bit tells the driver the commands allocated from this pool will be short lived and frequently recorded and reset.
            .queueFamilyIndex = (u32)renderstate->queue_family_indices.graphics_family
        };

        renderstate->old.onetime_command_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(renderstate->device, &onetime_cmdpool_create_info, NULL, &renderstate->old.onetime_command_pool));

        // Transfer command:
        renderstate->old.onetime_command = vklayer_alloc_cmd_buffer(renderstate->device, renderstate->old.onetime_command_pool);

        // Transfer fence:
        VkFenceCreateInfo onetime_fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        VK_CHECK(vkCreateFence(renderstate->device, &onetime_fence_create_info, NULL, &renderstate->old.onetime_command_complete_fence));

        SDL_Log("Created Transfer Command Pool for staging buffers\n");
    }

    // Create default sampler
    {
        renderstate->old.use_anisotropic_filtering = 0;

        VkSamplerCreateInfo sampler_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .magFilter         = VK_FILTER_LINEAR,
            .minFilter         = VK_FILTER_LINEAR,
            .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_LINEAR,  // aka trilinear filtering
            .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias        = 0.0f,  // Bias default=0, positive val shifts towards lower resolution mipmap. Negative -> towards higher res.
            .anisotropyEnable  = renderstate->old.use_anisotropic_filtering ? VK_TRUE : VK_FALSE,
            .maxAnisotropy     = renderstate->physical_device_properties.limits.maxSamplerAnisotropy,  // Set to devices max anisotropy (TODO: Haven't thought too hard about that decision to be honest)
            .compareEnable     = VK_FALSE,
            .compareOp         = VK_COMPARE_OP_NEVER,
            .minLod            = 0.0f,
            .maxLod            = VK_LOD_CLAMP_NONE,
            .borderColor       = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // Doesn't apply with repeat addressing mode.
            .unnormalizedCoordinates = VK_FALSE
        };

        renderstate->old.default_sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(renderstate->device, &sampler_create_info, NULL, &renderstate->old.default_sampler));
        

        SDL_Log("Created Default Sampler");
        if (renderstate->old.use_anisotropic_filtering) SDL_Log(" with max anisotropy of %f", sampler_create_info.maxAnisotropy);
        else SDL_Log(" with anisotropic filtering disabled!");
        SDL_Log("\n");

        SDL_assert(renderstate->old.default_sampler != VK_NULL_HANDLE);
    }
}

void old_stuff_clean(RenderState* renderstate)
{
    vkDestroySampler(renderstate->device, renderstate->old.default_sampler, NULL);  // Destroy default sampler

    // Destroy one time command stuff:
    vkDestroyFence(renderstate->device, renderstate->old.onetime_command_complete_fence, NULL);
    vkDestroyCommandPool(renderstate->device, renderstate->old.onetime_command_pool, NULL);
}
