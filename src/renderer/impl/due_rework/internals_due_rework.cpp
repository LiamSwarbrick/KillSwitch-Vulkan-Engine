#include "../internal_state.h"
#include "shared_glsl_defs.h"

VkDescriptorPool create_descriptor_pool(RenderState* renderstate, u32 max_descriptors, u32 max_sets);
VkDescriptorSet alloc_descriptor_set(RenderState* renderstate, VkDescriptorPool pool, VkDescriptorSetLayout set_layout);
void create_all_descriptor_set_layouts(RenderState* renderstate);
void destroy_all_descriptor_set_layouts(RenderState* renderstate);

const char*
get_render_mode_name(RenderMode render_mode)
{
    switch (render_mode)
    {
        #define AS_STRING(name, desc) case name: return #name;
        RENDERMODE_LIST(AS_STRING)
        #undef AS_STRING
        default: return "UNKNOWN_RENDERMODE";
    }
}

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

VkCommandBufferSubmitInfo vklayer_command_buffer_submit_info(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.commandBuffer = cmd;
    submit_info.deviceMask = 0;

    return submit_info;
}

VkSubmitInfo2 vklayer_submit_info(VkCommandBufferSubmitInfo* cmd_submit_info, VkSemaphoreSubmitInfo* signal_semaphore_submit_info, VkSemaphoreSubmitInfo* wait_semaphore_submit_info)
{
    VkSubmitInfo2 submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.pNext = NULL;

    submit_info.waitSemaphoreInfoCount = wait_semaphore_submit_info == NULL ? 0 : 1;
    submit_info.pWaitSemaphoreInfos = wait_semaphore_submit_info;

    submit_info.signalSemaphoreInfoCount = signal_semaphore_submit_info == NULL ? 0 : 1;
    submit_info.pSignalSemaphoreInfos = signal_semaphore_submit_info;

    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = cmd_submit_info;

    return submit_info;
}

VkImageSubresourceRange
vklayer_image_subresource_range(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange subimage = {};
    subimage.aspectMask = aspect_mask;
    subimage.baseMipLevel = 0;
    subimage.levelCount = VK_REMAINING_MIP_LEVELS;
    subimage.baseArrayLayer = 0;
    subimage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subimage;
}

VkImageMemoryBarrier2
vklayer_specify_image_transition_barrier(
    VkImage image,
    VkImageSubresourceRange subimage,
    // VkImageAspectFlags image_aspects_being_transitioned,

    // Before transition:
    VkPipelineStageFlags2 current_pipeline_stage,
    VkAccessFlags2        current_access_flags,
    VkImageLayout         current_layout,
    u32                   current_queue_family_index,

    // After transition:
    VkPipelineStageFlags2 new_pipeline_stage,
    VkAccessFlags2        new_access_flags,
    VkImageLayout         new_layout,
    u32                   new_queue_family_index

    // NOTE: Can default queue family indices to VK_QUEUE_FAMILY_IGNORED when not transfering between queues
)
{
    assert(image != VK_NULL_HANDLE);

    VkImageMemoryBarrier2 image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_barrier.pNext = NULL;

    // Specify what type of shader stages and memory accesses will wait for this transition.
    image_barrier.srcStageMask  = current_pipeline_stage;
    image_barrier.srcAccessMask = current_access_flags;
    image_barrier.dstStageMask  = new_pipeline_stage;
    image_barrier.dstAccessMask = new_access_flags;

    image_barrier.oldLayout = current_layout;
    image_barrier.newLayout = new_layout;

    // NOTE: We are not doing any ownership transfer of images between queues so keep these default:
    image_barrier.srcQueueFamilyIndex = current_queue_family_index;
    image_barrier.dstQueueFamilyIndex = new_queue_family_index;

    // // Specify which aspect of the image is changing e.g. color vs depth information
    // VkImageAspectFlags aspect_mask = image_aspects_being_transitioned;  // (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    // VkImageSubresourceRange subimage = vklayer_image_subresource_range(aspect_mask);

    image_barrier.image = image;
    image_barrier.subresourceRange = subimage;

    return image_barrier;
}

// NOTE: It is more efficient to allow multiple transitions at once instead of doing two sequential vkCmdPipelineBarrier's
//       so I'm implementing it like this instead of a transition single image function.
void
vklayer_cmd_transition_images(VkCommandBuffer cmd, u32 image_barrier_count, const VkImageMemoryBarrier2* image_barriers)
{
    assert(cmd != VK_NULL_HANDLE);
    assert(image_barriers);

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = NULL;

    // Allow the dependency to be satisfied per region rather than requiring the entire framebuffer to be complete.
    // E.g. so tiled-based GPUs can start processing the next subpass as soon as the same tile in the previous subpass is transitioned.
    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependency_info.imageMemoryBarrierCount = image_barrier_count;
    dependency_info.pImageMemoryBarriers = image_barriers;  // Specifies the old and new layouts

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkBufferMemoryBarrier2
vklayer_specify_buffer_barrier(
    VkBuffer buffer, VkDeviceSize buffer_size, VkDeviceSize buffer_offset,
    
    // Before barrirer
    VkPipelineStageFlags2 current_pipeline_stage,
    VkAccessFlags2        current_access_flags,
    u32                   current_queue_family_index,

    // After barrier:
    VkPipelineStageFlags2 new_pipeline_stage,
    VkAccessFlags2        new_access_flags,
    u32                   new_queue_family_index
)
{
    assert(buffer != VK_NULL_HANDLE);

    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.pNext = NULL;

    // Specify what type of shader stages and memory accesses will wait for this barrier
    barrier.srcStageMask = current_pipeline_stage;
    barrier.srcAccessMask = current_access_flags;
    barrier.dstStageMask = new_pipeline_stage;
    barrier.dstAccessMask = new_access_flags;
    
    barrier.srcQueueFamilyIndex = current_queue_family_index;
    barrier.dstQueueFamilyIndex = new_queue_family_index;

    barrier.buffer = buffer;
    barrier.offset = buffer_offset;
    barrier.size   = buffer_size;

    return barrier;
}

void
vklayer_cmd_pipeline_barrier_for_buffers(VkCommandBuffer cmd, u32 barrier_count, const VkBufferMemoryBarrier2* barriers)
{
    // NOTE: In cases where a pipeline barrier that transitions both barriers and images at once is needed.
    // Best to just specify the VkDependencyInfo manually instead of using the helper functions.
    // Because there is less overhead in using a single pipeline barrier (not sure how much overhead).

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = NULL;

    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependency_info.bufferMemoryBarrierCount = barrier_count;
    dependency_info.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkFormat vklayer_find_supported_depth_format(VkPhysicalDevice physical_device)
{

}

/////////////////

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

    // Create all descriptor set layouts
    // NOTE: Must be done before graphics pipeline creation
    create_all_descriptor_set_layouts(renderstate);
}

// TODO: Add function that handles the part of swapchain creation that is old, whereas the new stuff 
// will go in renderer.cpp, or for now, just put all of the swapchain shit in old.

// TODO: Then, expose the draw api, but only for a simple Renderer_DrawColoredCube() or something

void old_stuff_clean(RenderState* renderstate)
{
    destroy_all_descriptor_set_layouts(renderstate);
    vkDestroySampler(renderstate->device, renderstate->old.default_sampler, NULL);  // Destroy default sampler

    // Destroy one time command stuff:
    vkDestroyFence(renderstate->device, renderstate->old.onetime_command_complete_fence, NULL);
    vkDestroyCommandPool(renderstate->device, renderstate->old.onetime_command_pool, NULL);
}

void old_create_swapchain_tied_objects(RenderState* renderstate, VkFormat old_format)
{
    // Create render target
    VkExtent3D render_image_extent = {
        renderstate->swapchain_extent.width,
        renderstate->swapchain_extent.height,
        1
    };

    // Color buffer (final image) (Also an equivalent pingpong buffer for chaining postprocessing steps later)
    const VkFormat render_target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkImageUsageFlags render_target_usage = 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |                                // <- Colour attachment for fragment shader
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |  // <- Need to be able to copy rendered image to swapchain image
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;             // <- For compute and sampling as a texture during postprocessing
    renderstate->old.render_target_image          = create_render_target_attachment(renderstate, 0, render_target_format, render_image_extent, render_target_usage);
    renderstate->old.render_target_pingpong_image = create_render_target_attachment(renderstate, 0, render_target_format, render_image_extent, render_target_usage);
    renderstate->old.render_target_extent.width   = renderstate->old.render_target_image.image_extent.width;
    renderstate->old.render_target_extent.height  = renderstate->old.render_target_image.image_extent.height;

    // Depth
    renderstate->old.render_target_depth = create_render_target_attachment(renderstate, 1,
        VK_FORMAT_UNDEFINED,  // TODO: Depth format currently determine inside create_render_target_attachment() but maybe I should do it here instead
        render_image_extent,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT  // <- Depth-buffer used as G-Buffer in lighting pass.
    );

    // DEFERRED RENDERING:
    // G-Buffers for deferred rendering must be usable as both an attachment and a texture to be sampled from.
  
    renderstate->old.render_target_deferred_albedo_roughness = create_render_target_attachment(renderstate, 0,
        VK_FORMAT_R8G8B8A8_SRGB, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
    
    // NOTE: 10-bit normals caused lots of colour banding so only use the 16-bit format.
    renderstate->old.render_target_deferred_normal_metalness = create_render_target_attachment(renderstate, 0,
        VK_FORMAT_R16G16B16A16_SNORM, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
    // renderstate->old.render_target_deferred_normal_metalness = create_render_target_attachment(renderstate, 0, VK_FORMAT_A2B10G10R10_UNORM_PACK32, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);  // OLD.

    renderstate->old.render_target_deferred_emissive_ao = create_render_target_attachment(renderstate, 0,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        render_image_extent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );


    // SHADOW MAPS:
    const u32 shadow_map_resolution = 1024;
    renderstate->old.shadow_map_depth = create_render_target_attachment(renderstate, 1,
        VK_FORMAT_UNDEFINED,  // Currently this means it uses D32_SFLOAT
        (VkExtent3D){ shadow_map_resolution, shadow_map_resolution, 1 },
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );


    // Bloom target (lower res of render_target_image for extracting the bright parts)
    VkExtent3D bloom_target_extents = { render_image_extent.width / 2, render_image_extent.height / 2, 1 };
    renderstate->old.bloom_target_extent = { bloom_target_extents.width, bloom_target_extents.height };
    const VkFormat bloom_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkImageUsageFlags bloom_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    renderstate->old.bloom_target_image   = create_render_target_attachment(renderstate, 0, bloom_format, bloom_target_extents, bloom_usage);
    renderstate->old.bloom_pingpong_image = create_render_target_attachment(renderstate, 0, bloom_format, bloom_target_extents, bloom_usage);

    SDL_Log("- created render targets\n");

    

    // Graphics Pipeline
    //
    // NOTE: Only recreate graphics pipeline if format changed (no need to recreate it if only the extents changed).
    // Dynamic pipeline state let us handle the extent change (e.g. vkCmdSetViewport() and vkCmdSetScissor())
    // So we basically never recreate the graphics pipeline.
    //
    bool create_or_recreate_graphics_pipeline = (old_format != renderstate->swapchain_image_format);  // Format changed.
    if (create_or_recreate_graphics_pipeline)
    {
        vkDeviceWaitIdle(renderstate->device);
        
        destroy_all_graphics_pipelines(renderstate);
        create_all_graphics_pipelines(renderstate);
    }


    // Alloc postprocess descriptors and write to them
    renderstate->old.postprocess_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(renderstate->old.render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.render_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    renderstate->old.postprocess_pingpong_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(renderstate->old.render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.render_target_pingpong_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.postprocess_pingpong_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    renderstate->old.bloom_target_postprocess_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(renderstate->old.render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.bloom_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.bloom_target_postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    renderstate->old.bloom_pingpong_postprocess_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(renderstate->old.bloom_pingpong_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.bloom_pingpong_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.bloom_pingpong_postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    renderstate->old.bloom_apply_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.bloom_apply_set_layout);
    {
        VkWriteDescriptorSet descriptors[BLOOM_APPLY_DESCRIPTOR_COUNT] = {};
        assert(renderstate->old.render_target_image.image);
        assert(renderstate->old.bloom_target_image.image);

        VkDescriptorImageInfo scene_image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.render_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo bloom_image_info = {
            .sampler     = renderstate->old.postprocess_sampler,
            .imageView   = renderstate->old.bloom_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.bloom_apply_descriptor_set; 
        descriptors[0].dstBinding      = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_SCENE_TEXTURE; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &scene_image_info;

        descriptors[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[1].pNext           = NULL;
        descriptors[1].dstSet          = renderstate->old.bloom_apply_descriptor_set; 
        descriptors[1].dstBinding      = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_BLOOM_TEXTURE; 
        descriptors[1].descriptorCount = 1; 
        descriptors[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[1].pImageInfo      = &bloom_image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    // Alloc GBuffer descriptor sets and write to them
    renderstate->old.gbuffers_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.gbuffers_set_layout);
    {
        VkWriteDescriptorSet descriptors[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
        VkDescriptorImageInfo image_infos[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
        for (int i = 0; i < RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT; ++i)
        {
            GPU_Image* attachment;
            if (i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT)
                attachment = &renderstate->old.render_target_deferred_attachments[i];
            else
                attachment = &renderstate->old.render_target_depth;
            assert(attachment->image_view);

            image_infos[i] = {
                .sampler     = renderstate->old.postprocess_sampler,
                .imageView   = attachment->image_view,
                .imageLayout = i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };

            descriptors[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[i].pNext           = NULL;
            descriptors[i].dstSet          = renderstate->old.gbuffers_descriptor_set; 
            descriptors[i].dstBinding      = GBUFFERS_DESCRIPTOR_SET_BINDING_ALBEDO_ROUGHNESS + i;
            descriptors[i].descriptorCount = 1; 
            descriptors[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptors[i].pImageInfo      = &image_infos[i];
        }
        
        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }

    // Alloc Shadow map descriptor sets and write to them
    renderstate->old.shadow_maps_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.shadow_maps_set_layout);
    {
        VkWriteDescriptorSet descriptors[1] = {};
        assert(renderstate->old.render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = renderstate->old.shadow_map_sampler,
            .imageView   = renderstate->old.shadow_map_depth.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = renderstate->old.shadow_maps_descriptor_set; 
        descriptors[0].dstBinding      = SHADOW_MAPS_DESCRIPTOR_SET_BINDING;
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
    }
}

void old_destroy_swapchain_tied_objects(RenderState* renderstate)
{
    // Destroy render targets
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.render_target_image);
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.render_target_pingpong_image);
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.render_target_depth);

    // Destroy G Buffers
    for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
        destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.render_target_deferred_attachments[i]);

    // Shadow map and bloom target
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.shadow_map_depth);
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.bloom_target_image);
    destroy_image(renderstate->device, renderstate->vma_allocator, renderstate->old.bloom_pingpong_image);
}

//////////////////////////


VkDescriptorPool create_descriptor_pool(RenderState* renderstate, u32 max_descriptors, u32 max_sets)
{
    // List of the kinds of descriptors that the pool should contain
    VkDescriptorPoolSize const pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_descriptors },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_descriptors },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_descriptors }
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = max_sets,
        .poolSizeCount = sizeof(pool_sizes)/sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(renderstate->device, &pool_create_info, NULL, &pool));

    return pool;
}

VkDescriptorSet alloc_descriptor_set(RenderState* renderstate, VkDescriptorPool pool, VkDescriptorSetLayout set_layout)
{
    VkDescriptorSetAllocateInfo set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout
    };

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(renderstate->device, &set_alloc_info, &descriptor_set));
    
    return descriptor_set;
}

void create_all_descriptor_set_layouts(RenderState* renderstate)
{
    // Allocate a large pool of descriptors and descriptor sets so we don't run out
    renderstate->old.descriptor_pool = create_descriptor_pool(renderstate, 2048, 1024);

    // Scene descriptor set layout
    {
        // Create uniform buffer for scene
        renderstate->old.scene_uniform_buffer = create_buffer(
            renderstate->vma_allocator,
            sizeof(SceneUniform_GLSL_ScalarBlock),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        // Set bindings for descriptor set layouts
        VkDescriptorSetLayoutBinding bindings[1] = {
            {
                .binding         = SCENE_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = SCENE_DESCRIPTOR_COUNT,
                .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_GEOMETRY_BIT,
                .pImmutableSamplers = NULL
            }
        };

        // Create scene set layout
        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .bindingCount = sizeof(bindings) / sizeof(bindings[0]),
            .pBindings = bindings
        };

        renderstate->old.scene_set_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.scene_set_layout));


        // Alloc scene descriptors and write to it
        renderstate->old.scene_descriptor_set = alloc_descriptor_set(renderstate, renderstate->old.descriptor_pool, renderstate->old.scene_set_layout);
        {
            VkWriteDescriptorSet descriptors[SCENE_DESCRIPTOR_COUNT] = {};

            // Scene uniforms
            VkDescriptorBufferInfo scene_ubo_info = {
                .buffer = renderstate->old.scene_uniform_buffer.buffer,
                .offset = 0,
                .range = sizeof(SceneUniform_GLSL_ScalarBlock)
            };
            descriptors[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[0].pNext = NULL;
            descriptors[0].dstSet           = renderstate->old.scene_descriptor_set;
            descriptors[0].dstBinding       = SCENE_DESCRIPTOR_SET_BINDING_UNIFORMS;
            descriptors[0].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            descriptors[0].descriptorCount  = SCENE_DESCRIPTOR_COUNT;  // We bind each descriptor to a single binding point
            descriptors[0].pBufferInfo      = &scene_ubo_info;

            // Update descriptor sets for the scene
            // NOTE: (only a single uniform buffer for scene right now)
            const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
            vkUpdateDescriptorSets(renderstate->device, num_sets, descriptors, 0, NULL);
        }
    }

    // Create object descriptor set layout
    {
        // TODO: Small refactor so that the assert checking isn't run time (aka the static assert way I use for the G-Buffers)
        VkDescriptorSetLayoutBinding bindings[OBJECT_DESCRIPTOR_COUNT] = {};

        u32 bindings_implemented = 0;
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 0) uniform sampler2D rgba_albedo_alpha_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_ALBEDO_ALPHA_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 1) uniform sampler2D rgb_roughness_metalness_ao_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_RMA_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 2) uniform sampler2D rgb_normal_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_NORMAL_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 3) uniform sampler2D rgb_emissive_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_EMISSIVE_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };

        // Error checking: makes it easier to debug if we add more but forget to specify them in the layout here.
        assert(bindings_implemented == OBJECT_DESCRIPTOR_COUNT);

        // Create object set layout
        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding),
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.object_set_layout));
    }

    // Create postprocess descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = POSTPROCESS_DESCRIPTOR_COUNT,
            .pBindings     = bindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.postprocess_set_layout));

        // Create postprocess sampler
        {
            VkSamplerCreateInfo sampler_create_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
            #if 0  // FOR MOSAIC FILTER
                .magFilter         = VK_FILTER_NEAREST,
                .minFilter         = VK_FILTER_NEAREST, 
                .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            #else  // FOR BLOOM WE WANT LINEAR INSTEAD, of course this means the mosaic is no longer correct at the moment, but it's fine. The whole thing needs replacing with a simpler rendergraph system at some point anyway.
                .magFilter         = VK_FILTER_LINEAR,
                .minFilter         = VK_FILTER_LINEAR,
                .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            #endif
                .addressModeW      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .mipLodBias        = 0.0f,
                .anisotropyEnable  = VK_FALSE,
                .maxAnisotropy     = 0.0f,
                .compareEnable     = VK_FALSE,
                .compareOp         = VK_COMPARE_OP_NEVER,
                .minLod            = 0.0f,
                .maxLod            = VK_LOD_CLAMP_NONE,
                .borderColor       = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                .unnormalizedCoordinates = VK_FALSE
            };

            renderstate->old.postprocess_sampler = VK_NULL_HANDLE;
            VK_CHECK(vkCreateSampler(renderstate->device, &sampler_create_info, NULL, &renderstate->old.postprocess_sampler));
        }
    }

    // Create Bloom Apply descriptor set layout
    // NOTE: For now, just use postprocess_sampler with it, when writing descriptor sets
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_SCENE_TEXTURE,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_BLOOM_TEXTURE,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = BLOOM_APPLY_DESCRIPTOR_COUNT,
            .pBindings     = bindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.bloom_apply_set_layout));
    }

    // Create G-Buffer samplers descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_ALBEDO_ROUGHNESS,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_NORMAL_METALNESS,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_EMISSIVE_AO,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_DEPTH,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };
        static_assert(sizeof(bindings)/sizeof(bindings[0]) == RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT);

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.gbuffers_set_layout));

        // NOTE: Currently using the postprocess sampler for GBuffer reads as well.
    }

    // Create lights descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = LIGHTS_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = 1,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.lights_set_layout));
    }

    // Create shadow maps descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = SHADOW_MAPS_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = 1,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate->device, &layout_create_info, NULL, &renderstate->old.shadow_maps_set_layout));

        // Create shadow map sampler
        VkSamplerCreateInfo sampler_create_info = {  // TODO: What sampling options should i use for shadows
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .magFilter         = VK_FILTER_LINEAR,  // Hardware PCF (Percentage closer filtering) when linear is combined with compareOp VK_TRUE
            .minFilter         = VK_FILTER_LINEAR,
            .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,

            // Must clamp to border opaque white so that stuff outside the shadow map is considered unshadowed.
            .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias        = 0.0f,
            .anisotropyEnable  = VK_FALSE,
            .maxAnisotropy     = 0.0f,

            // Hardware shadow comparison
            .compareEnable     = VK_TRUE,
            .compareOp         = VK_COMPARE_OP_LESS_OR_EQUAL,  // LESS because not using reverse-z right now

            .minLod            = 0.0f,
            .maxLod            = VK_LOD_CLAMP_NONE,
            .borderColor       = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE
        };

        renderstate->old.shadow_map_sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(renderstate->device, &sampler_create_info, NULL, &renderstate->old.shadow_map_sampler));
    }
}

void destroy_all_descriptor_set_layouts(RenderState* renderstate)
{
    // // Destroy lights descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.lights_set_layout, NULL);
    // for (int i = 0; i < FRAME_OVERLAP; ++i)
    // {
    //     destroy_buffer(renderstate->vma_allocator, &renderstate->frames[i].lights_buffer);
    // }
    #warning TODO: destroy lights buffer during frame destroy

    // Destroy scene set layout and uniform buffer
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.scene_set_layout, NULL);
    vmaDestroyBuffer(renderstate->vma_allocator, renderstate->old.scene_uniform_buffer.buffer, renderstate->old.scene_uniform_buffer.allocation);

    // Destroy object descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.object_set_layout, NULL);

    // Destroy post process descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.postprocess_set_layout, NULL);
    vkDestroySampler(renderstate->device, renderstate->old.postprocess_sampler, NULL);

    // Destroy bloom descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.bloom_apply_set_layout, NULL);

    // Destroy gbuffers descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.gbuffers_set_layout, NULL);   

    // Destroy shadow map descriptor set layout
    vkDestroyDescriptorSetLayout(renderstate->device, renderstate->old.shadow_maps_set_layout, NULL);
    vkDestroySampler(renderstate->device, renderstate->old.shadow_map_sampler, NULL);

    // Destroy descriptor pool
    vkDestroyDescriptorPool(renderstate->device, renderstate->old.descriptor_pool, NULL);
}


VkCommandBuffer begin_one_time_command(RenderState* renderstate)
{
    // Reset the one time command pool
    VK_CHECK(vkResetCommandPool(renderstate->device, renderstate->old.onetime_command_pool, 0));
    VK_CHECK(vkResetFences(renderstate->device, 1, &renderstate->old.onetime_command_complete_fence));

    // Begin recording
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    VK_CHECK(vkBeginCommandBuffer(renderstate->old.onetime_command, &begin_info));

    // NOTE: No need to return currently since we know we are using renderstate->old.onetime_comand,
    // but if we change this to use more than one command buffer
    // this will make the refactoring easier later, so use the returned value.
    return renderstate->old.onetime_command;
}

void end_one_time_command_and_wait(RenderState* renderstate, VkCommandBuffer command)
{
    // NOTE: For now, we just use one pool and one command for this, so assert that...
    assert(command == renderstate->old.onetime_command);

    VK_CHECK(vkEndCommandBuffer(command));

    // Submit commands
    VkCommandBufferSubmitInfo command_submit_info = vklayer_command_buffer_submit_info(command);
    VkSubmitInfo2 submit = vklayer_submit_info(&command_submit_info, NULL, NULL);
    VK_CHECK(vkQueueSubmit2(renderstate->graphics_queue, 1, &submit, renderstate->old.onetime_command_complete_fence));

    // CPU waits for queued command to finish by waiting on the command complete fence
    VK_CHECK(vkWaitForFences(renderstate->device, 1, &renderstate->old.onetime_command_complete_fence, VK_TRUE, UINT64_MAX));
}


GPU_Image create_image_texture2d(RenderState* renderstate, u8* data, u64 data_size, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage)
{
    // Uploads data to video memory via a staging buffer.
    // Always generates mipmaps.

    const u32 mip_levels = compute_num_mip_levels(width, height);
    VkExtent3D extent = { width, height, 1 };

    // NOTE: We require these to upload image data.
    assert(usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT && usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.flags = 0;
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateImage(renderstate->vma_allocator, &image_create_info, &alloc_create_info, &image, &allocation, NULL));


    // Load image data into video memory via staging buffer.
    GPU_Buffer staging_buffer = create_staging_buffer_from_data(renderstate->vma_allocator, data, data_size);
    VkCommandBuffer transfer_command = begin_one_time_command(renderstate);
    {
        // Transition whole image layout to TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier2 image_as_transfer_dst_barrier = vklayer_specify_image_transition_barrier(
            image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
            /* Before*/
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &image_as_transfer_dst_barrier);

        // Copy staging buffer to image base mip level (level 0)
        VkImageSubresourceLayers image_subresource_layers = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkBufferImageCopy copy = {
            .bufferOffset       = 0,
            .bufferRowLength    = 0,
            .bufferImageHeight  = 0,
            .imageSubresource   = image_subresource_layers,
            .imageOffset        = { 0, 0, 0 },
            .imageExtent        = { width, height, 1 }
        };
        vkCmdCopyBufferToImage(transfer_command, staging_buffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        // Now the base mip level has the image data, create the rest of the mip levels with blits
        // (Blits allow downsampling).

        // First transition base mip level to TRANSFER_SRC_OPTIMAL
        // (We already transitioned the remaining mip levels to TRANSFER_DST_OPTIMAL with the last image barrier)
        VkImageSubresourceRange base_subimage = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel    = 0,
            .levelCount      = 1,
            .baseArrayLayer  = 0,
            .layerCount      = 1
        };
        VkImageMemoryBarrier2 baselevel_to_transfer_src_barrier = vklayer_specify_image_transition_barrier(
            image, base_subimage,
            /* Before */
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &baselevel_to_transfer_src_barrier);

        // Process all mip levels with blits
        // Each subsequent blit copies at half of previous size.

        s32 current_width  = (s32)width;
        s32 current_height = (s32)height;
        // NOTE: Signed ints because thats what VkOffset3D uses.

        for (u32 level = 1; level < mip_levels; ++level)
        {
            // Blit previous mipmap level (level-1) to the current level.

            // Source is the previous mip level
            VkImageBlit blit_regions = {};
            blit_regions.srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level-1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            blit_regions.srcOffsets[0] = { 0, 0, 0 };
            blit_regions.srcOffsets[1] = { current_width, current_height, 1 };

            // Next mip level is half the size
            current_width  /= 2;
            current_height /= 2;

            // Dest is the current mip level
            blit_regions.dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            blit_regions.dstOffsets[0] = { 0, 0, 0 };
            blit_regions.dstOffsets[1] = { current_width, current_height, 1 };
            
            // Blit command:
            vkCmdBlitImage(transfer_command,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit_regions,
                VK_FILTER_LINEAR  // Linear filter is required for the mip mapping's averaging effect
            );

            // Transition this mip level to be a TRANSFER_SRC for the next iteration.
            // NOTE: We technically don't need to transfer the last mip level, but it simplifies
            // the final image barrier if the whole image ends up in the same format after this loop.
            VkImageSubresourceRange miplevel_i_subimage = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = level,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            VkImageMemoryBarrier2 miplevel_to_src_barrier = vklayer_specify_image_transition_barrier(
                image, miplevel_i_subimage,
                /* Before */
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                /* After */
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED
            );
            vklayer_cmd_transition_images(transfer_command, 1, &miplevel_to_src_barrier);
        }

        // Finally, now that all mip levels are filled with image data
        // Transition the whole image into format to be read by the fragment shader
        VkImageMemoryBarrier2 make_image_shader_read_ready_barrier = vklayer_specify_image_transition_barrier(
            image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
            /* Before*/
            VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &make_image_shader_read_ready_barrier);

    }
    end_one_time_command_and_wait(renderstate, transfer_command);

    // Clean up staging buffer
    destroy_buffer(renderstate->vma_allocator, &staging_buffer);


    // Create image view
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VkImageView image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(renderstate->device, &image_view_create_info, NULL, &image_view));

    GPU_Image gpu_image = {
        .image = image,
        .image_view = image_view,
        .allocation = allocation,
        .image_extent = extent,
        .image_format = format
    };

    return gpu_image;
}

GPU_Image create_render_target_attachment(RenderState* renderstate, b32 is_depth_attachment, VkFormat desired_format, VkExtent3D extent, VkImageUsageFlags usage)
{
    VkFormat format;
    // VkImageUsageFlags usage;
    VkImageAspectFlags aspect_flags;

    // NOTE: If is_depth_attachment, desired_format must be VK_FORMAT_UNDEFINED since we find whatever the best format based on the device
    if (is_depth_attachment)
    {
        assert(
            desired_format == VK_FORMAT_UNDEFINED &&
            "For depth, we find the best format automatically, so desired_format arg should be VK_FORMAT_UNDEFINED."
        );
    }

    if (is_depth_attachment)
    {
        // Get best depth format for this device
        format = vklayer_find_supported_depth_format(renderstate->physical_device);
        // usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // Include the depth aspect.
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
        
        // TODO: Check for stencil format and | with VK_IMAGE_ASPECT_STENCIL_BIT
    }
    else  // Color attachment
    {
        format = desired_format;

        // usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        //         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        //         VK_IMAGE_USAGE_SAMPLED_BIT;  // <- Last one is for postprocessing
        
        aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    return create_attachment_image(renderstate, extent, format, usage, aspect_flags, 0);  // No MSAA
}

GraphicsPipeline create_graphics_pipeline(RenderState* renderstate, GraphicsPipelineConfigInfo config)
{
    assert(renderstate->old.render_target_image.image != VK_NULL_HANDLE && "graphics pipeline relies on using the same image format as the render target image");

    GraphicsPipeline gfx = {
        .is_created = 1,
        .layout = VK_NULL_HANDLE,
        .pipeline = VK_NULL_HANDLE,
        .has_attrib = {}
    };
    for (u32 i = 0; i < sizeof(gfx.has_attrib) / sizeof(gfx.has_attrib[0]); ++i)
        gfx.has_attrib[i] = config.has_attrib[i];

    // Create pipeline layout
    VK_CHECK(vkCreatePipelineLayout(renderstate->device, &config.pipeline_layout_create_info, NULL, &gfx.layout));

    // Load shader code spirv files
    // - Using VK_KHR_maintenance5 feature of Vulkan 1.4 to skip VkShaderModule and instead pass spirv directly to the pipeline object.

    u32 num_shaders = 0;  // To count how many shaders files are in config
    
    u64 vert_spirv_size = 0;
    u32* vert_spirv_code = NULL;
    if (config.vertex_spirv_config.spirv_path)
    {
        ++num_shaders;
        vert_spirv_code = (u32*)L_load_binary_file(config.vertex_spirv_config.spirv_path, &vert_spirv_size, &renderstate->main.tt);
        if (vert_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.vertex_spirv_config.spirv_path);
            exit(1);
        }
    }
    
    u64 frag_spirv_size = 0;
    u32* frag_spirv_code = NULL;
    if (config.fragment_spirv_config.spirv_path)
    {
        ++num_shaders;
        frag_spirv_code = (u32*)L_load_binary_file(config.fragment_spirv_config.spirv_path, &frag_spirv_size, &renderstate->main.tt);
        if (frag_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.fragment_spirv_config.spirv_path);
            exit(1);
        }
    }

    u64 geom_spirv_size = 0;
    u32* geom_spirv_code = NULL;
    if (config.geometry_spirv_config.spirv_path)
    {
        ++num_shaders;
        geom_spirv_code = (u32*)L_load_binary_file(config.geometry_spirv_config.spirv_path, &geom_spirv_size, &renderstate->main.tt);
        if (geom_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.geometry_spirv_config.spirv_path);
            exit(1);
        }
    }
    
    // Vertex and fragment shaders are the minimum for a graphics pipeline
    if (!config.vertex_spirv_config.spirv_path || !config.fragment_spirv_config.spirv_path)
    {
        fprintf(stderr, "Missing vertex or fragment shader path in config: Those two are the minimum required for a graphics pipeline.\n");
        exit(1);
    }


    // Put spirv into shader module create infos
    // Pass that into shader stages via the pNext chain
    assert(num_shaders >= 2);

    VkShaderModuleCreateInfo* code = (VkShaderModuleCreateInfo*)L_calloc(num_shaders, sizeof(VkShaderModuleCreateInfo), &renderstate->main.tt);  // C++ lacking variable length arrays be like
    VkPipelineShaderStageCreateInfo* stages = (VkPipelineShaderStageCreateInfo*)L_calloc(num_shaders, sizeof(VkPipelineShaderStageCreateInfo), &renderstate->main.tt);

    u32 stage_count = 0;

    // Vertex shader module and stage
    const u32 spirv_vertex_index = stage_count++;
    code[spirv_vertex_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    code[spirv_vertex_index].pNext     = NULL;
    code[spirv_vertex_index].flags     = 0;
    code[spirv_vertex_index].codeSize  = vert_spirv_size;
    code[spirv_vertex_index].pCode     = vert_spirv_code;

    stages[spirv_vertex_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[spirv_vertex_index].pNext   = &code[spirv_vertex_index];
    stages[spirv_vertex_index].flags   = 0;
    stages[spirv_vertex_index].stage   = VK_SHADER_STAGE_VERTEX_BIT;
    stages[spirv_vertex_index].module  = VK_NULL_HANDLE;
    stages[spirv_vertex_index].pName   = config.vertex_spirv_config.entrypoint_name;
    stages[spirv_vertex_index].pSpecializationInfo = config.vertex_spirv_config.pSpecializationInfo;

    // Geometry shader (Order of these shaders matters in the code and stages arrays, vertex->geometry->fragment)
    if (geom_spirv_code)
    {
        const u32 spirv_geometry_index = stage_count++;
        // Geometry shader module and stage
        code[spirv_geometry_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        code[spirv_geometry_index].pNext     = NULL;
        code[spirv_geometry_index].flags     = 0;
        code[spirv_geometry_index].codeSize  = geom_spirv_size;
        code[spirv_geometry_index].pCode     = geom_spirv_code;    

        stages[spirv_geometry_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[spirv_geometry_index].pNext   = &code[spirv_geometry_index];
        stages[spirv_geometry_index].flags   = 0;
        stages[spirv_geometry_index].stage   = VK_SHADER_STAGE_GEOMETRY_BIT;
        stages[spirv_geometry_index].module  = VK_NULL_HANDLE;
        stages[spirv_geometry_index].pName   = config.geometry_spirv_config.entrypoint_name;
        stages[spirv_geometry_index].pSpecializationInfo = config.geometry_spirv_config.pSpecializationInfo;
    }

    // Fragment shader module and stage
    const u32 spirv_fragment_index = stage_count++;
    code[spirv_fragment_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    code[spirv_fragment_index].pNext     = NULL;
    code[spirv_fragment_index].flags     = 0;
    code[spirv_fragment_index].codeSize  = frag_spirv_size;
    code[spirv_fragment_index].pCode     = frag_spirv_code;    

    stages[spirv_fragment_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[spirv_fragment_index].pNext   = &code[spirv_fragment_index];
    stages[spirv_fragment_index].flags   = 0;
    stages[spirv_fragment_index].stage   = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[spirv_fragment_index].module  = VK_NULL_HANDLE;
    stages[spirv_fragment_index].pName   = config.fragment_spirv_config.entrypoint_name;
    stages[spirv_fragment_index].pSpecializationInfo = config.fragment_spirv_config.pSpecializationInfo;


    // Vertex attributes
    u32 attrib_count = 0;
    u32 binding_count = 0;
    VkVertexInputBindingDescription vertex_inputs[MAX_VERTEX_ATTRIBUTES];
    VkVertexInputAttributeDescription vertex_attributes[MAX_VERTEX_BINDINGS];
    {
        // TODO: For instanced rendering look into VK_VERTEX_INPUT_RATE_INSTANCE

        // Position (vec3f)
        if (config.has_attribute_position)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_POSITION;
            vertex_inputs[binding_count].stride    = sizeof(float) * 3;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_POSITION;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_POSITION;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Normal (vec3f)
        if (config.has_attribute_normal)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_NORMAL;
            vertex_inputs[binding_count].stride    = sizeof(float) * 3;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_NORMAL;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_NORMAL;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Texcoord (vec2f)
        if (config.has_attribute_texcoord)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_TEXCOORD;
            vertex_inputs[binding_count].stride    = sizeof(float) * 2;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_TEXCOORD;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_TEXCOORD;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Tangent (vec4f)
        if (config.has_attribute_tangent)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_TANGENT;
            vertex_inputs[binding_count].stride    = sizeof(float) * 4;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_TANGENT;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_TANGENT;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32A32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.pNext = NULL;
    vertex_input_state_create_info.flags = 0;
    vertex_input_state_create_info.vertexBindingDescriptionCount    = binding_count;
    vertex_input_state_create_info.pVertexBindingDescriptions       = vertex_inputs;
    vertex_input_state_create_info.vertexAttributeDescriptionCount  = attrib_count;
    vertex_input_state_create_info.pVertexAttributeDescriptions     = vertex_attributes;

    // Input Assembly (the rasterization primitive)
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
    input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.pNext = NULL;
    input_assembly_create_info.flags = 0;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor regions
    VkViewport viewport = {};  // The framebuffer coords the ndcs map to
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)renderstate->old.render_target_extent.width;
    viewport.height = (float)renderstate->old.render_target_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};  // The subset of the framebuffer to actual rasterize to
    scissor.offset = (VkOffset2D){ 0, 0 };
    scissor.extent = renderstate->old.render_target_extent;

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.pNext = NULL;
    viewport_state_create_info.flags = 0;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.pViewports = &viewport;
    viewport_state_create_info.scissorCount = 1;
    viewport_state_create_info.pScissors = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo raster_state_create_info = {};
    raster_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_state_create_info.pNext = NULL;
    raster_state_create_info.flags = 0;
    raster_state_create_info.depthClampEnable = VK_FALSE;
    raster_state_create_info.rasterizerDiscardEnable = VK_FALSE;  // This would disable rasterization of our primitives so we definitely don't want that.
    // For example, there are things like Point-Tesselated Voxelization (Fei, et al. 2012) that use hardware tesselation but not use the rasterizer.
    raster_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_state_create_info.cullMode = config.cull_mode;
    raster_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_state_create_info.depthBiasEnable = VK_FALSE;
    raster_state_create_info.lineWidth = 1.0f;  // Required. (Because it only applies to line raster primitives or VK_POLYGON_MODE_LINE)
    
    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {};
    multisampling_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_state_create_info.pNext = NULL;
    multisampling_state_create_info.flags = 0;
    multisampling_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil buffer state
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .depthTestEnable        = VK_TRUE,
        .depthWriteEnable       = VK_TRUE,
        .depthCompareOp         = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable  = VK_FALSE,  // NOTE: I think depth bounds is for that CAT-scan like effect where you only see a portion of the depth range rendered.
        .stencilTestEnable      = VK_FALSE,  // NOTE: Not using stencil buffer at the moment
        .front                  = {},
        .back                   = {},
        .minDepthBounds         = 0.0f,
        .maxDepthBounds         = 1.0f
    };
    for (u32 i = 0; i < config.blend_mode_count; ++i)
    {
        if (config.blend_modes[i] == BLEND_MODE_ALPHA_BLEND)
        {
            // It's incorrect to write depth values of translucent fragments (except for BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH)
            // So if any color attachment using alpha blending we disable depth writing.
            depth_stencil_state_create_info.depthWriteEnable = VK_FALSE;
        }
    }

    // Color blend state (one per attachment)
    VkPipelineColorBlendAttachmentState* blend_states = (VkPipelineColorBlendAttachmentState*)L_calloc(config.blend_mode_count,  sizeof(VkPipelineColorBlendAttachmentState), &renderstate->main.tt);
    for (u32 i = 0; i < config.blend_mode_count; ++i)
    {
        BlendMode blend_mode = config.blend_modes[i];
        switch (blend_mode)
        {
            // NOTE: Not setting blend_states[i].alphaBlendOp. No purpose for it yet.

            case BLEND_MODE_OPAQUE:
            case BLEND_MODE_ALPHA_MASK:
                blend_states[i].blendEnable = VK_FALSE;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;
            
            case BLEND_MODE_ALPHA_BLEND:
            case BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH:
                blend_states[i].blendEnable          = VK_TRUE;
                blend_states[i].colorBlendOp         = VK_BLEND_OP_ADD;
                blend_states[i].srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_states[i].dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;

            case BLEND_MODE_ADD_TO_SRC:
                blend_states[i].blendEnable = VK_TRUE;
                blend_states[i].colorBlendOp = VK_BLEND_OP_ADD;
                blend_states[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_states[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;

            default:
                assert(0 && "Unimplemented blend mode.");
                abort();
        }
    }
    
    VkPipelineColorBlendStateCreateInfo blend_state_create_info = {};
    blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state_create_info.pNext = NULL;
    blend_state_create_info.flags = 0;
    blend_state_create_info.logicOpEnable = VK_FALSE;
    blend_state_create_info.attachmentCount = config.blend_mode_count;
    blend_state_create_info.pAttachments = blend_states;

    // Dynamic States
    // Specifically scissor and viewport so we don't have to recreate the pipeline on every swapchain resize.
    // Although we'd still have to recreate the pipeline if the swapchain images change format (but that's much less frequent).
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.pNext = NULL;
    dynamic_state_create_info.flags = 0;

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    dynamic_state_create_info.dynamicStateCount = sizeof(dynamic_states)/sizeof(VkDynamicState);
    dynamic_state_create_info.pDynamicStates = dynamic_states;


    // Graphics Pipeline Create Info
    // All that shit goes into the graphics pipeline create info
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &config.pipeline_rendering_create_info,  // NOTE: pNext chain instead of Vulkan render passes.
        .flags = 0,
        .stageCount           = stage_count,
        .pStages              = stages,
        .pVertexInputState    = &vertex_input_state_create_info,
        .pInputAssemblyState  = &input_assembly_create_info,
        .pTessellationState   = NULL,  // TODO: Not using tesselation at the moment.
        .pViewportState       = &viewport_state_create_info,
        .pRasterizationState  = &raster_state_create_info,
        .pMultisampleState    = &multisampling_state_create_info,
        .pDepthStencilState   = &depth_stencil_state_create_info,
        .pColorBlendState     = &blend_state_create_info,
        .pDynamicState        = &dynamic_state_create_info,

        .layout = gfx.layout,

        // Not using render passes:
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(renderstate->device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &gfx.pipeline));


    L_free(blend_states, &renderstate->main.tt);

    // Free loaded shader files and allocated create infos
    if (vert_spirv_code) L_free(vert_spirv_code, &renderstate->main.tt);
    if (frag_spirv_code) L_free(frag_spirv_code, &renderstate->main.tt);
    if (geom_spirv_code) L_free(geom_spirv_code, &renderstate->main.tt);
    L_free(code,   &renderstate->main.tt);
    L_free(stages, &renderstate->main.tt);


    SDL_Log("Graphics Pipeline Created.\n");

    return gfx;
}


void destroy_graphics_pipeline(RenderState* renderstate, GraphicsPipeline* gp)
{
    if (gp->is_created)
    {
        assert(gp->layout);
        assert(gp->pipeline);
        vkDestroyPipelineLayout(renderstate->device, gp->layout, NULL);
        vkDestroyPipeline(renderstate->device, gp->pipeline, NULL);
        
        *gp = {};
        gp->is_created = 0;  // Just being explicit
        gp->layout = VK_NULL_HANDLE;
        gp->pipeline = VK_NULL_HANDLE;
    }
}

static GraphicsPipeline create_fullscreen_postprocess_pipeline(RenderState* renderstate, const char* fragment_spirv_path, const VkFormat output_image_format)
{
    const char* vertex_spirv_path   = SPIRV_PATH_FULLSCREEN_VERT;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        renderstate->old.scene_set_layout,       // set 0
        renderstate->old.postprocess_set_layout  // set 1
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { output_image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    SDL_Log("Fullscreen postprocess ");
    return create_graphics_pipeline(renderstate, config);
}

static GraphicsPipeline create_bloom_apply_pipeline(RenderState* renderstate, VkFormat target_format)
{
    const char* vertex_spirv_path    = SPIRV_PATH_FULLSCREEN_VERT;
    const char* fragment_spirv_path  = SPIRV_PATH_BLOOM_APPLY_FRAG;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        renderstate->old.scene_set_layout,       // set 0
        renderstate->old.bloom_apply_set_layout  // set 1
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { target_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    SDL_Log("Fullscreen postprocess (apply bloom) ");
    return create_graphics_pipeline(renderstate, config);
}

static GraphicsPipeline create_deferred_opaque_geometry_pipeline(RenderState* renderstate)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_DEFERRED_WRITE_GBUFFERS_FRAG;

    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        renderstate->old.scene_set_layout,   // set 0
        renderstate->old.object_set_layout,  // set 1
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Specify G-Buffer attachments (except depth attachment which is specified seperately in VkPipelineRenderingCreateInfo):
    VkFormat color_formats[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
    for (u32 i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
    {
        color_formats[i] = renderstate->old.render_target_deferred_attachments[i].image_format;
    }

    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
    for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
        blend_modes[i] = BLEND_MODE_OPAQUE;

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    SDL_Log("Deferred opaque geometry ");
    return create_graphics_pipeline(renderstate, config);
}

static GraphicsPipeline create_deferred_lighting_pass_pipeline(RenderState* renderstate)
{
    const char* vertex_spirv_path   = SPIRV_PATH_FULLSCREEN_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_DEFERRED_LIGHTING_FRAG;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        renderstate->old.scene_set_layout,       // set 0
        renderstate->old.gbuffers_set_layout,    // set 1
        renderstate->old.lights_set_layout,      // set 2
        renderstate->old.shadow_maps_set_layout  // set 3
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { renderstate->old.render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    SDL_Log("Deferred lighting ");
    return create_graphics_pipeline(renderstate, config);
}

static GraphicsPipeline create_forward_opaque_pass(RenderState* renderstate)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_FORWARD_LIGHTING_FRAG;

    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        renderstate->old.scene_set_layout,   // set 0
        renderstate->old.object_set_layout,  // set 1
        renderstate->old.lights_set_layout,  // set 2
        renderstate->old.shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { renderstate->old.render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    SDL_Log("Forward opaque ");
    return create_graphics_pipeline(renderstate, config);
}

static GraphicsPipeline create_forward_transparent_pass(RenderState* renderstate)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_FORWARD_LIGHTING_FRAG;

    // Pass specialization constants
    b32 is_alpha_masked = true;
    VkSpecializationMapEntry map_entry_is_alpha_masked = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(VkBool32)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_is_alpha_masked,
        .dataSize       = sizeof(VkBool32),
        .pData          = &is_alpha_masked
    };
    
    // TEMP: For the leaf sway....
    VkSpecializationInfo vertex_specialization_info = fragment_specialization_info;

    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        renderstate->old.scene_set_layout,   // set 0
        renderstate->old.object_set_layout,  // set 1
        renderstate->old.lights_set_layout,  // set 2
        renderstate->old.shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    const VkFormat color_formats[] = { renderstate->old.render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { BLEND_MODE_ALPHA_MASK };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &vertex_specialization_info
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_NONE  // Disable backface culling for alpha-masked leaves
    };

    SDL_Log("Forward transparent ");
    return create_graphics_pipeline(renderstate, config);
}

static void create_general_debug_visuals_pipelines(RenderState* renderstate)
{
    // Debug rendermodes:
    SDL_Log("Debug Viz %s\n", get_render_mode_name(renderstate->old.render_mode));

    const char* vertex_spirv_path    = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path  = SPIRV_PATH_DEBUG_VISUALS_FRAG;

    // NOTE: Google's GLSLC doesn't support custom GLSL entrypoints so we do this by hand with a specialization constant for now.
    s32 fragment_entrypoint_id = 0;
    VkSpecializationMapEntry map_entry_fragment_entrypoint_id = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(fragment_entrypoint_id)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_fragment_entrypoint_id,
        .dataSize       = sizeof(fragment_entrypoint_id),
        .pData          = &fragment_entrypoint_id
    };

    BlendMode blend_mode_opaque_geometry = BLEND_MODE_OPAQUE;
    BlendMode blend_mode_transparent_geometry = BLEND_MODE_OPAQUE;

    // Select shader and graphics pipeline parameters based on render mode
    switch (renderstate->old.render_mode)
    {
        case RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_MIPLEVELS;
            break;

        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH;
            break;

        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH_DERIVATIVES;
            break;
        
        case RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASELINE_OVERDRAW;

            blend_mode_opaque_geometry = BLEND_MODE_ALPHA_BLEND;
            blend_mode_transparent_geometry = BLEND_MODE_ALPHA_BLEND;
            break;

        case RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASIC_OVERSHADING;

            blend_mode_opaque_geometry = BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH;  // Write the depth just like how our opaque geometry is tested against in the lighting shaders
            blend_mode_transparent_geometry = BLEND_MODE_ALPHA_BLEND;             // Depth testing does not happen in our transparent frag shader so we maintain that here.
            break;

        default:
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error: Unimplemented general debug render mode: %d\n", renderstate->old.render_mode);
            assert(0 && "Unimplemented render mode");
            exit(1);
    }

    
    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        renderstate->old.scene_set_layout,   // set 0
        renderstate->old.object_set_layout,  // set 1
        renderstate->old.lights_set_layout,  // set 2
        renderstate->old.shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { renderstate->old.render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { blend_mode_opaque_geometry };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    SDL_Log("[\n  Opaque ");
    renderstate->old.gp_opaque_pass = create_graphics_pipeline(renderstate, config);

    SDL_Log("  Transparent ");
    config.blend_modes[0] = blend_mode_transparent_geometry;
    renderstate->old.gp_transparent_pass = create_graphics_pipeline(renderstate, config);
    SDL_Log("]\n");
}

static void create_mesh_density_visualisation_pipelines(RenderState* renderstate)
{
    // Debug rendermodes:
    SDL_Log("Debug Viz %s\n", get_render_mode_name(renderstate->old.render_mode));

    const char* vertex_spirv_path    = SPIRV_PATH_DEBUG_MESH_DENSITY_VERT;
    const char* geometry_spirv_path  = SPIRV_PATH_DEBUG_MESH_DENSITY_GEOM;
    const char* fragment_spirv_path  = SPIRV_PATH_DEBUG_MESH_DENSITY_FRAG;

    BlendMode blend_mode_opaque_geometry = BLEND_MODE_OPAQUE;
    BlendMode blend_mode_transparent_geometry = BLEND_MODE_OPAQUE;
    
    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        renderstate->old.scene_set_layout,       // set 0
        renderstate->old.gbuffers_set_layout,    // set 1
        renderstate->old.lights_set_layout,      // set 2
        renderstate->old.shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { renderstate->old.render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = renderstate->old.render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { blend_mode_opaque_geometry };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .geometry_spirv_config = {
            .spirv_path          = geometry_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },

        .has_attribute_position  = 1,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    SDL_Log("[\n  Opaque ");
    renderstate->old.gp_opaque_pass = create_graphics_pipeline(renderstate, config);

    SDL_Log("  Transparent ");
    config.blend_modes[0] = blend_mode_transparent_geometry;
    renderstate->old.gp_transparent_pass = create_graphics_pipeline(renderstate, config);
    SDL_Log("]\n");
}

static GraphicsPipeline create_shader_mapping_pipeline(RenderState* renderstate, b32 is_transparent)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SHADOW_MAP_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_SHADOW_MAP_FRAG;

    // Pass specialization constants
    b32 is_alpha_masked = is_transparent;
    VkSpecializationMapEntry map_entry_is_alpha_masked = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(VkBool32)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_is_alpha_masked,
        .dataSize       = sizeof(VkBool32),
        .pData          = &is_alpha_masked
    };
    
    // TEMP: To capture the leaf sway in the shadow mapping of alpha masked folliage
    VkSpecializationInfo vertex_specialization_info = fragment_specialization_info;

    // Define which descripter set layouts the shaders use
    assert(renderstate->old.scene_set_layout != VK_NULL_HANDLE);
    assert(renderstate->old.object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        renderstate->old.scene_set_layout,  // set 0
        renderstate->old.object_set_layout  // set 1
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // No color formats
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = 0,
        .pColorAttachmentFormats  = NULL,
        .depthAttachmentFormat    = renderstate->old.shadow_map_depth.image_format,  // TODO: When using multiple shadow maps, they should all be the same format so do: shadow_maps[0].format prolly
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_mode = is_transparent ? BLEND_MODE_ALPHA_MASK : BLEND_MODE_OPAQUE;

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &vertex_specialization_info
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 1,  // NOTE: Texcoords only needed for alpha masked pipeline, but we include it in opaque as well so we don't have to write to seperate shader files
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = 1,
        .blend_modes = &blend_mode,
        .cull_mode = is_transparent ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT  // Disable backface culling for alpha-masked leaves
    };

    SDL_Log("Shadow map %s ", is_transparent ? "transparent" : "opaque");
    return create_graphics_pipeline(renderstate, config);
}


void create_all_graphics_pipelines(RenderState* renderstate)
{
    SDL_Log("Creating all graphics pipelines:\n");

    // Main geometry and lighting pipelines depends on rendermode:
    switch (renderstate->old.render_mode)
    {
        case RENDERMODE_DEFAULT_DEFERRED:
            renderstate->old.gp_opaque_pass = create_deferred_opaque_geometry_pipeline(renderstate);
            renderstate->old.gp_deferred_lighting = create_deferred_lighting_pass_pipeline(renderstate);
            renderstate->old.gp_transparent_pass = create_forward_transparent_pass(renderstate);
            break;
        
        case RENDERMODE_DEFAULT_FORWARD:
            renderstate->old.gp_opaque_pass = create_forward_opaque_pass(renderstate);
            renderstate->old.gp_transparent_pass = create_forward_transparent_pass(renderstate);
            break;
        
        case RENDERMODE_DEBUGVIZ_MESH_DENSITY:
            create_mesh_density_visualisation_pipelines(renderstate);
            break;
        
        case RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS:
        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH:
        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES:        
        case RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW:
        case RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING:
            create_general_debug_visuals_pipelines(renderstate);
            break;

        default:
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error: Unimplemented render mode: %d\n", renderstate->old.render_mode);
            assert(0 && "Unimplemented render mode");
            exit(1);
    }

    // Postprocess shaders
    renderstate->old.gp_fullscreen_tri_mosaic = create_fullscreen_postprocess_pipeline(renderstate, SPIRV_PATH_POSTPROCESS_MOSAIC_FRAG, renderstate->swapchain_image_format);

    // Bloom shaders (horizontal goes into an identical pingpong image buffer because we can't read and write to the same image at the same time during parallel computation)
    renderstate->old.gp_bloom_brightness_extraction    = create_fullscreen_postprocess_pipeline(renderstate, SPIRV_PATH_BLOOM_EXTRACT_BRIGHTNESS_FRAG, renderstate->old.bloom_target_image.image_format);
    renderstate->old.gp_bloom_gaussian_blur_horizontal = create_fullscreen_postprocess_pipeline(renderstate, SPIRV_PATH_BLOOM_HBLUR_FRAG, renderstate->old.bloom_pingpong_image.image_format);
    renderstate->old.gp_bloom_gaussian_blur_vertical   = create_fullscreen_postprocess_pipeline(renderstate, SPIRV_PATH_BLOOM_VBLUR_FRAG, renderstate->old.bloom_target_image.image_format);
    renderstate->old.gp_bloom_apply = create_bloom_apply_pipeline(renderstate, renderstate->old.render_target_pingpong_image.image_format);

    // Shadow mapping shaders
    renderstate->old.gp_shadowmap_opaque      = create_shader_mapping_pipeline(renderstate, 0);
    renderstate->old.gp_shadowmap_transparent = create_shader_mapping_pipeline(renderstate, 1);
}

void destroy_all_graphics_pipelines(RenderState* renderstate)
{
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_opaque_pass);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_deferred_lighting);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_transparent_pass);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_fullscreen_tri_mosaic);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_shadowmap_opaque);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_shadowmap_transparent);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_bloom_brightness_extraction);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_bloom_gaussian_blur_horizontal);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_bloom_gaussian_blur_vertical);
    destroy_graphics_pipeline(renderstate, &renderstate->old.gp_bloom_apply);
}

