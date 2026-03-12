#include "framegraph.h"

#include "internal_state.h"

void bindless_heap_init();
void bindless_heap_shutdown();
void bindless_heap_create_all_samplers();

void fg_add_barrier(FG_Resource* res, PassResourceUsage* usage, 
                   VkImageMemoryBarrier2* img_barriers, uint32_t* img_count,
                   VkBufferMemoryBarrier2* buf_barriers, uint32_t* buf_count);
void fg_apply_barriers(VkCommandBuffer cmd, RenderPassDesc* pass);
void fg_execute_pass(uint32_t pass_idx, VkCommandBuffer cmd);

typedef struct NewResourceInfo
{
    ResourceImportInfo import_info;
    VmaAllocation allocation;  // NOTE: Imported resources don't have a VMA allocation so should just use VK_NULL_HANDLE here.
}
NewResourceInfo;
uint32_t add_resource_to_registry_and_heap(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, NewResourceInfo resource_info);

// Init Subsystem
//

void FG_Init()
{
    bindless_heap_init();
    
    // Create global pipeline layout that uses this heap
    {
        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = 128  // NOTE: Vulkan 1.4 guaruntees 256, before that is only 128. So we could make it 256 if we never intend on supporting earlier versions.
        };

        VkPipelineLayoutCreateInfo layout_create_info = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext           = NULL,
            .flags           = 0,
            .setLayoutCount  = 1,
            .pSetLayouts     = &renderstate.heap.set_layout,
            .pushConstantRangeCount  = 1,
            .pPushConstantRanges     = &push_constant_range
        };

        VK_CHECK(vkCreatePipelineLayout(renderstate.device, &layout_create_info, NULL, &renderstate.global_pipeline_layout));
    }
}

void FG_Shutdown()
{
    bindless_heap_shutdown();
    vkDestroyPipelineLayout(renderstate.device, renderstate.global_pipeline_layout, NULL);
}

void FG_ClearResources()
{
    vkDeviceWaitIdle(renderstate.device);

    for (uint32_t i = 0; i < renderstate.registry.resource_count; ++i)
    {
        FG_Resource* res = &renderstate.registry.resources[i];

        FG_DeallocateResource(res);
    }

    // Reset counters so the next "Create" call starts from index 0
    renderstate.registry.resource_count = 0;
    renderstate.heap.texture_count = 0;
    
    // Note: We don't destroy the global_set or samplers here because 
    // those live for the lifetime of the engine (FG_Init / FG_Shutdown).
    
    SDL_Log("FrameGraph Resources Destroyed & Registry Reset.");
}


// Graph Building
//

void FG_Empty()
{
    // Empty pass descriptions
    renderstate.framegraph.pass_count = 0;
    memset(renderstate.framegraph.passes, 0, sizeof(renderstate.framegraph.passes));

    // Empty table
    for (uint32_t i = 0; i < PASS_TYPE_COUNT; ++i)
    {
        renderstate.pass_id_from_type[i] = PASS_TYPE_INVALID;
    }
}

uint32_t FG_AddPass(RenderPassDesc pass_description, uint32_t pass_type)
{
    FrameGraph* fg = &renderstate.framegraph;
    SDL_assert(fg->pass_count < MAX_PASSES);

    // Copy pass description to the next slot and return the index.
    uint32_t pass_id = fg->pass_count++;
    RenderPassDesc* pass = &fg->passes[pass_id];
    memcpy(pass, &pass_description, sizeof(RenderPassDesc));

    // Add to table
    SDL_assert(renderstate.pass_id_from_type[pass_type] == PASS_TYPE_INVALID &&
        "Can't add the same pass twice, give it a unique PassType enumeration"
    );
    renderstate.pass_id_from_type[pass_type] = pass_id;

    return pass_id;
}

// Graph Execution
//

void fg_add_barrier(FG_Resource* res, PassResourceUsage* usage, 
                   VkImageMemoryBarrier2* img_barriers, uint32_t* img_count,
                   VkBufferMemoryBarrier2* buf_barriers, uint32_t* buf_count)
{
    // Skip if state already matches (e.g. resource used in same state in consecutive passes)
    if (res->current_layout == usage->layout && 
        res->current_access == usage->access && 
        res->current_stage  == usage->stage)
    {
        return;
    }

    // Add to barrers array and update resource with post-transition sync state
    if (res->type == FG_RESOURCE_TYPE_IMAGE)
    {
        img_barriers[(*img_count)++] = (VkImageMemoryBarrier2){
            .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext                = NULL,
            .srcStageMask         = res->current_stage,
            .srcAccessMask        = res->current_access,
            .dstStageMask         = usage->stage,
            .dstAccessMask        = usage->access,
            .oldLayout            = res->current_layout,
            .newLayout            = usage->layout,
            .srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
            .image                = res->image.handle,
            .subresourceRange     = res->image.subresource_range
        };
        res->current_layout = usage->layout;
    } 
    else
    {
        buf_barriers[(*buf_count)++] = (VkBufferMemoryBarrier2){
            .sType                = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext                = NULL,
            .srcStageMask         = res->current_stage,
            .srcAccessMask        = res->current_access,
            .dstStageMask         = usage->stage,
            .dstAccessMask        = usage->access,
            .srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
            .buffer               = res->buffer.handle,
            .offset               = 0,
            .size                 = res->buffer.size
        };
    }
    res->current_stage = usage->stage;
    res->current_access = usage->access;
}

void fg_apply_barriers(VkCommandBuffer cmd, RenderPassDesc* pass)
{
    VkImageMemoryBarrier2 image_barriers[MAX_PASS_RESOURCE_BANDWIDTH * 2];  // *2 because each resource could require both an input and output barrier.
    VkBufferMemoryBarrier2 buffer_barriers[MAX_PASS_RESOURCE_BANDWIDTH * 2];
    uint32_t img_count = 0;
    uint32_t buf_count = 0;

    // Transition INPUTS
    for (uint32_t i = 0; i < pass->input_count; i++)
    {
        FG_Resource* res = &renderstate.registry.resources[pass->inputs[i].rid];
        fg_add_barrier(res, &pass->inputs[i], image_barriers, &img_count, buffer_barriers, &buf_count);
    }

    // Transition OUTPUTS
    for (uint32_t i = 0; i < pass->output_count; i++)
    {
        FG_Resource* res = &renderstate.registry.resources[pass->outputs[i].rid];
        fg_add_barrier(res, &pass->outputs[i], image_barriers, &img_count, buffer_barriers, &buf_count);
    }

    if (img_count > 0 || buf_count > 0)
    {
        VkDependencyInfo dep = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = NULL,
            
            // VK_DEPENDENCY_BY_REGION_BIT is used on tiled GPUs for example.
            // E.g. the barrier will be local to each tile instead of stalling the whole pipeline.
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,

            .memoryBarrierCount        = 0,
            .pMemoryBarriers           = NULL,
            .bufferMemoryBarrierCount  = buf_count,
            .pBufferMemoryBarriers     = buffer_barriers,
            .imageMemoryBarrierCount   = img_count,
            .pImageMemoryBarriers      = image_barriers
        };
        vkCmdPipelineBarrier2(cmd, &dep);
    }
}

void fg_execute_pass(uint32_t pass_idx, VkCommandBuffer cmd)
{
    FrameGraph* fg = &renderstate.framegraph;
    RenderPassDesc* pass = &fg->passes[pass_idx];
    fg_apply_barriers(cmd, pass);
    
    if (pass->is_compute)
    {
        // Compute passes don't use vkCmdBeginRendering
        pass->execute_callback(cmd, pass->user_data);
    }
    else  // Graphics Pass:
    {
        // Fill attachment infos for each output resource in pass->outputs
        VkRenderingAttachmentInfo color_attachments[MAX_PASS_RESOURCE_BANDWIDTH] = {};
        VkRenderingAttachmentInfo depth_attachment = {};
        VkRenderingAttachmentInfo stencil_attachment = {};

        uint32_t color_attachment_count = 0;
        b32 has_depth = 0;
        b32 has_stencil = 0;

        for (uint32_t i = 0; i < pass->output_count; ++i)
        {
            PassResourceUsage* usage = &pass->outputs[i];

            SDL_assert(usage->rid < renderstate.registry.resource_count);
            FG_Resource* res = &renderstate.registry.resources[usage->rid];

            VkRenderingAttachmentInfo attachment = {
                .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext               = NULL,
                .imageView           = res->image.view,
                .imageLayout         = usage->layout,

                // Currently no MSAA
                .resolveMode         = VK_RESOLVE_MODE_NONE,
                .resolveImageView    = VK_NULL_HANDLE,
                .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                .loadOp              = usage->load_op,
                .storeOp             = usage->store_op,
                .clearValue          = usage->clear_value
            };

            if (usage->usage_flags & FG_USAGE_COLOR)
            {
                color_attachments[color_attachment_count++] = attachment;
            }

            // NOTE: Packed Depth-Stencil uses the same ImageView but different aspects in the barrier
            if (usage->usage_flags & FG_USAGE_DEPTH)
            {
                depth_attachment = attachment;
                has_depth = 1;
            }
            if (usage->usage_flags & FG_USAGE_STENCIL)
            {
                stencil_attachment = attachment;
                has_stencil = 1;
            }
        }

        VkRenderingInfo render_info = {
            .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                 = NULL,
            .flags                 = 0,
            .renderArea            = pass->render_area,
            .layerCount            = 1,
            .viewMask              = 0,
            .colorAttachmentCount  = color_attachment_count,
            .pColorAttachments     = color_attachments,
            .pDepthAttachment      = has_depth   ? &depth_attachment   : NULL,
            .pStencilAttachment    = has_stencil ? &stencil_attachment : NULL
        };

        vkCmdBeginRendering(cmd, &render_info);

        // Dynamic pipeline states: Set viewport and scissor.
        if (pass->use_custom_viewport_scissor)
        {
            vkCmdSetViewport(cmd, 0, 1, &pass->custom_viewport);
            vkCmdSetScissor(cmd, 0, 1, &pass->custom_scissor);
        }
        else
        {
            // Default: Match the render area
            VkViewport default_vp = {
                .x = (float)pass->render_area.offset.x,
                .y = (float)pass->render_area.offset.y,
                .width  = (float)pass->render_area.extent.width,
                .height = (float)pass->render_area.extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            vkCmdSetViewport(cmd, 0, 1, &default_vp);
            vkCmdSetScissor(cmd, 0, 1, &pass->render_area);
        }

        pass->execute_callback(cmd, pass->user_data);

        vkCmdEndRendering(cmd);
    }
}

void FG_CmdRenderFrame(VkCommandBuffer cmd)
{
    SDL_assert(!renderstate.registry.dirty_because_gaps);

#ifndef NDEBUG
    // Validation:
    for (uint32_t i = 0; i < renderstate.registry.resource_count; ++i)
    {
        SDL_assert(renderstate.registry.resources[i].type != FG_RESOURCE_TYPE_INVALID);
    }
#endif

    FrameGraph* fg = &renderstate.framegraph;
    SDL_assert(fg->pass_count < MAX_PASSES);

    for (uint32_t i = 0; i < fg->pass_count; ++i)
    {
        fg_execute_pass(i, cmd);
    }
}

void FG_CmdTransitionSwapchainForPresentation(VkCommandBuffer cmd, uint32_t swapchain_image_rid)
{
    SDL_assert(swapchain_image_rid < renderstate.registry.resource_count);
    FG_Resource* swapchain_resource = &renderstate.registry.resources[swapchain_image_rid];

    VkPipelineStageFlags2 new_stage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        new_access = VK_ACCESS_2_NONE;
    VkImageLayout         new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // Move swapchain image to present queue with present format.
    VkImageMemoryBarrier2 barrier = {
        .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext                = NULL,
        .srcStageMask         = swapchain_resource->current_stage,
        .srcAccessMask        = swapchain_resource->current_access,
        .dstStageMask         = new_stage,
        .dstAccessMask        = new_access,
        .oldLayout            = swapchain_resource->current_layout,
        .newLayout            = new_layout,
        .srcQueueFamilyIndex  = renderstate.queue_family_indices.graphics_family,
        .dstQueueFamilyIndex  = renderstate.queue_family_indices.present_family,
        .image                = swapchain_resource->image.handle,
        .subresourceRange     = swapchain_resource->image.subresource_range
    };
    VkDependencyInfo dependency = {
        .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                     = NULL,
        .dependencyFlags           = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount   = 1,
        .pImageMemoryBarriers      = &barrier
    };
    vkCmdPipelineBarrier2(cmd, &dependency);

    // Keep track of this new state.
    // NOTE: Even though swapchain presenting is the last step of rendering.
    //       We still must update the resources current state, since this
    //       swapchain image resource gets used again a few frames later.
    swapchain_resource->current_stage  = new_stage;
    swapchain_resource->current_access = new_access;
    swapchain_resource->current_layout = new_layout;
}

// Resource Tracking
//

uint32_t add_resource_to_registry_and_heap(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, NewResourceInfo resource_info)
{
    SDL_assert(renderstate.registry.resource_count < MAX_RESOURCES);

    uint32_t id = UINT32_MAX;
    if (renderstate.registry.dirty_because_gaps)
    {
        b32 found_gap = 0;

        // Linear search to find empty slot (but keep going to end of array to check if it still has gaps
        for (uint32_t i = 0; i < renderstate.registry.resource_count; ++i)
        {
            if (renderstate.registry.resources[i].type == FG_RESOURCE_TYPE_INVALID)
            {
                if (found_gap)
                {
                    break;  // We now know the registry is still dirty, so no resetting the dirty flag
                }

                id = i;
                found_gap = 1;
            }

            if (i == renderstate.registry.resource_count-1)
            {
                // We have searched the whole array, and found no more gaps
                renderstate.registry.dirty_because_gaps = 0;
            }
        }
    }

    if (id == UINT32_MAX)
    {
        id = renderstate.registry.resource_count++;
    }
    
    FG_Resource* res = &renderstate.registry.resources[id];

    // For safety, zero the struct, but this function should manully set all parameters so it shouldn't matter.
    memset(res, 0, sizeof(FG_Resource));

    // Set shared fields
    strncpy(res->debug_name, debug_name, sizeof(res->debug_name));
    res->type = type;
    res->flags = flags;
    res->allocation = resource_info.allocation;
    res->current_access = VK_ACCESS_2_NONE;
    res->current_stage  = VK_PIPELINE_STAGE_2_NONE;
    res->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (type == FG_RESOURCE_TYPE_BUFFER)
    {
        printf("Adding BUFFER resource to registry. (" ANSI_CYAN "%s" ANSI_RESET ")\n", res->debug_name);
        res->buffer = resource_info.import_info.buffer;

        // Immediately grab the BDA pointer
        VkBufferDeviceAddressInfo address_info = {
            .sType   = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext   = NULL,
            .buffer  = res->buffer.handle,
        };
        res->buffer_gpu_address = vkGetBufferDeviceAddress(renderstate.device, &address_info);
    }
    else
    {
        printf("Adding IMAGE resource to registry. (" ANSI_CYAN "%s" ANSI_RESET ")\n", res->debug_name);
        res->image = resource_info.import_info.image;
        
        // Only images created with SAMPLED_BIT should go in the BindlessHeap
        // For imported resources (the swapchain), this will be false.
        b32 can_be_sampled = res->image.usage & VK_IMAGE_USAGE_SAMPLED_BIT;

        if (can_be_sampled)
        {
            printf("Adding " ANSI_CYAN "%s" ANSI_RESET " to heap (is samplable).\n", res->debug_name);

            // Register in  bindless descriptor array
            res->image_bindless_index = renderstate.heap.texture_count++;
            VkDescriptorImageInfo descriptor_image_info = {
                .imageView    = res->image.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            VkWriteDescriptorSet descriptor_write = {
                .sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext             = NULL,
                .dstSet            = renderstate.heap.global_set,
                .dstBinding        = 0,  // <- Take note, images array is binding=0
                .dstArrayElement   = res->image_bindless_index,
                .descriptorCount   = 1,
                .descriptorType    = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo        = &descriptor_image_info,
                .pBufferInfo       = NULL,
                .pTexelBufferView  = NULL
            };
            vkUpdateDescriptorSets(renderstate.device, 1, &descriptor_write, 0, NULL);
        }
        else
        {
            // UINT32_MAX to represent nonsamplable images not being part of the bindless heap.
            res->image_bindless_index = UINT32_MAX;
        }
    }

    // Returns the resource's id into the registry
    return id;
}

uint32_t FG_CreateResource(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, ResourceCreateInfo* create_info)
{
    NewResourceInfo resource_info = {};

    if (type == FG_RESOURCE_TYPE_BUFFER)
    {
        // Create .buffer.handle, with allocation stored in .allocation
        VmaAllocationCreateInfo alloc_create_info = { .usage = VMA_MEMORY_USAGE_AUTO };
        VK_CHECK(vmaCreateBuffer(
            renderstate.vma_allocator,
            &create_info->buffer_create_info,
            &alloc_create_info,
            &resource_info.import_info.buffer.handle,
            &resource_info.allocation,
            NULL
        ));
        
        // Keep metadata
        resource_info.import_info.buffer.size = create_info->buffer_create_info.size;
        resource_info.import_info.buffer.mapped_data = NULL;  // TODO! vmaMapMemory stuff might be useful for CPU side updating of skeletal bones.
    }
    else
    {
        // Create .image.handle, with allocation stored in .allocation
        VmaAllocationCreateInfo alloc_create_info = { .usage=VMA_MEMORY_USAGE_AUTO };
        VK_CHECK(vmaCreateImage(
            renderstate.vma_allocator,
            &create_info->image_create_info,
            &alloc_create_info, 
            &resource_info.import_info.image.handle,
            &resource_info.allocation,
            NULL
        ));

        // Now we actually have the image handle and can set it in the view's create info
        SDL_assert(create_info->image_view_create_info.image == VK_NULL_HANDLE &&
            "Just leave image_view_create_info's image handle null when passing to FG_CreateResource"
        );
        create_info->image_view_create_info.image = resource_info.import_info.image.handle;
        
        // Create .image.view
        VK_CHECK(vkCreateImageView(
            renderstate.device,
            &create_info->image_view_create_info,
            NULL,
            &resource_info.import_info.image.view
        ));
        
        // Keep metadata
        resource_info.import_info.image.format = create_info->image_create_info.format;
        resource_info.import_info.image.extent = create_info->image_create_info.extent;
        resource_info.import_info.image.usage  = create_info->image_create_info.usage;
        resource_info.import_info.image.subresource_range = create_info->image_view_create_info.subresourceRange;
    }

    // Returns the resource's id into the registry
    return add_resource_to_registry_and_heap(debug_name, type, flags, resource_info);
}

uint32_t FG_ImportResource(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, ResourceImportInfo import_info)
{
    NewResourceInfo resource_info = {
        .import_info = import_info,
        .allocation = VK_NULL_HANDLE
    };

    // Returns the resource's id into the registry
    return add_resource_to_registry_and_heap(debug_name, type, flags, resource_info);
}

// NOTE: Only for internal use, this leaves in gaps in the registry which breaks things if you don't replace the hole.
void FG_DeallocateResource(FG_Resource* res)
{
    // Imported resources won't have a VMA allocation, they are managed by whoever imported them.
    if (res->allocation != VK_NULL_HANDLE)
    {
        if (res->type == FG_RESOURCE_TYPE_BUFFER)
        {
            vmaDestroyBuffer(renderstate.vma_allocator, res->buffer.handle, res->allocation);
        }
        else
        {
            SDL_assert(res->image.view != VK_NULL_HANDLE);
            vkDestroyImageView(renderstate.device, res->image.view, NULL);

            vmaDestroyImage(renderstate.vma_allocator, res->image.handle, res->allocation);
        }
    }

    // Wipe the struct memory to prevent accidental reuse
    memset(res, 0, sizeof(FG_Resource));
    res->type = FG_RESOURCE_TYPE_INVALID;  // Just to be explicit

    // Set the dirty flag since there is now a gap in the registry
    renderstate.registry.dirty_because_gaps = 1;
}

// Staging
//

void FG_UploadBufferData(ThreadStagingObjects* stg, uint32_t rid, const void* data, uint32_t size)
{
    FG_Resource* res = &renderstate.registry.resources[rid];
    SDL_assert(res->type == FG_RESOURCE_TYPE_BUFFER);
    SDL_assert(size <= res->buffer.size);

    VkBuffer      staging_buf   = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;

    VkBufferCreateInfo staging_buffer_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                   = size,
        .usage                  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode            = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo alloc_create_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VmaAllocationInfo mapped_info;
    VK_CHECK(vmaCreateBuffer(renderstate.vma_allocator, &staging_buffer_create_info, &alloc_create_info, &staging_buf, &staging_alloc, &mapped_info));

    // Copy data to the mapped memory region
    memcpy(mapped_info.pMappedData, data, size);

    // Record the copy command from mapped staging region to device local memory of the resource
    vkResetCommandBuffer(stg->upload_command_buffer, 0);
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(stg->upload_command_buffer, &begin_info);

    VkBufferCopy copy_region = { .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(stg->upload_command_buffer, staging_buf, res->buffer.handle, 1, &copy_region);

    vkEndCommandBuffer(stg->upload_command_buffer);

    // Submit and Wait
    // NOTE: For the baseline, this uses a temporary fence to avoid GPU-side race conditions,
    // i.e., this thread must wait CPU side so that we don't try use the command buffer while it's in use.
    // I've heard of better strategies to hide latency but meh. Can just put this on non-main threads.
    VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    vkCreateFence(renderstate.device, &fence_info, NULL, &fence);

    thread_safe_submit_cmd(stg->upload_command_buffer, fence);
    vkWaitForFences(renderstate.device, 1, &fence, VK_TRUE, UINT64_MAX);
    
    vkDestroyFence(renderstate.device, fence, NULL);
    vmaDestroyBuffer(renderstate.vma_allocator, staging_buf, staging_alloc);
}

void FG_UploadImageData(ThreadStagingObjects* stg, uint32_t rid, const void* data, uint32_t size)
{
    FG_Resource* res = &renderstate.registry.resources[rid];
    SDL_assert(res->type == FG_RESOURCE_TYPE_IMAGE);
    SDL_assert(res->image.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT && "To copy into an image resource, it must have the TRANSFER_DST usage flag");

    VkDeviceSize image_size = size;

    VkBuffer staging_buf;
    VmaAllocation staging_alloc;

    VkBufferCreateInfo staging_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VmaAllocationInfo mapped_info;
    VK_CHECK(vmaCreateBuffer(renderstate.vma_allocator, &staging_info, &alloc_info, &staging_buf, &staging_alloc, &mapped_info));

    // Copy data to the mapped memory region
    memcpy(mapped_info.pMappedData, data, size);

    // Record command to transfer staged data to image buffer
    vkResetCommandBuffer(stg->upload_command_buffer, 0);
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(stg->upload_command_buffer, &begin_info);

    // Transition to transfer destination
    VkImageMemoryBarrier2 barrier = {
        .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext                = NULL,
        .srcStageMask         = res->current_stage,
        .srcAccessMask        = res->current_access,
        .dstStageMask         = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask        = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout            = res->current_layout,
        .newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
        .image                = res->image.handle,
        .subresourceRange     = res->image.subresource_range
    };
    VkDependencyInfo dep_info = {
        .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                     = NULL,
        .dependencyFlags           = 0,
        .imageMemoryBarrierCount   = 1,
        .pImageMemoryBarriers      = &barrier
    };
    vkCmdPipelineBarrier2(stg->upload_command_buffer, &dep_info);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },

        .imageOffset = {0,0,0},
        .imageExtent = res->image.extent
    };

    vkCmdCopyBufferToImage(
        stg->upload_command_buffer,
        staging_buf,
        res->image.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    // Transition back
    VkImageMemoryBarrier2 barrier_b = {
        .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext                = NULL,
        .srcStageMask         = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask        = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask         = res->current_stage,
        .dstAccessMask        = res->current_access,
        .oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout            = res->current_layout,
        .srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
        .image                = res->image.handle,
        .subresourceRange     = res->image.subresource_range
    };
    VkDependencyInfo dep_info_b = {
        .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                     = NULL,
        .dependencyFlags           = 0,
        .imageMemoryBarrierCount   = 1,
        .pImageMemoryBarriers      = &barrier
    };
    vkCmdPipelineBarrier2(stg->upload_command_buffer, &dep_info_b);

    vkEndCommandBuffer(stg->upload_command_buffer);

    
    VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    vkCreateFence(renderstate.device, &fence_info, NULL, &fence);

    thread_safe_submit_cmd(stg->upload_command_buffer, fence);
    vkWaitForFences(renderstate.device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(renderstate.device, fence, NULL);
    vmaDestroyBuffer(renderstate.vma_allocator, staging_buf, staging_alloc);
}


// Descriptors
//

void bindless_heap_init()
{
    // Create Layout
    {
        // Binding 0: Texture Array (Sampled Images)
        // Binding 1: Sampler Array
        VkDescriptorSetLayoutBinding bindings[2] = {
            {
                .binding             = 0,
                .descriptorType      = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount     = NUM_BINDLESS_TEXTURE_SLOTS,
                .stageFlags          = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers  = NULL
            },
            {
                .binding             = 1,
                .descriptorType      = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount     = FG_SAMPLER_COUNT,
                .stageFlags          = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers  = NULL
            }
        };

        VkDescriptorBindingFlags flags[2] = {
            // Sampled Images:
            // The array of image descriptors won't be full. And we want update which are used depending on renderpass.
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,

            // Samplers:
            // Just fully bind the fixed array array of samplers before command buffer recording:
            0
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_create_info = {
            .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext          = NULL,
            .bindingCount   = 2,
            .pBindingFlags  = flags
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = &flags_create_info,
            .flags         = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount  = 2,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(renderstate.device, &layout_create_info, NULL, &renderstate.heap.set_layout));
    }

    // Create descriptor pool and allocate global_set
    {
        VkDescriptorPoolSize pool_sizes[2] = {
            { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = NUM_BINDLESS_TEXTURE_SLOTS },
            { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = FG_SAMPLER_COUNT }
        };

        VkDescriptorPoolCreateInfo pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext          = NULL,
            .flags          = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets        = 1,
            .poolSizeCount  = 2,
            .pPoolSizes     = pool_sizes
        };
        VK_CHECK(vkCreateDescriptorPool(renderstate.device, &pool_create_info, NULL, &renderstate.heap.descriptor_pool));

        VkDescriptorSetAllocateInfo set_alloc_info = {
            .sType               = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext               = NULL,
            .descriptorPool      = renderstate.heap.descriptor_pool,
            .descriptorSetCount  = 1,
            .pSetLayouts         = &renderstate.heap.set_layout
        };
        vkAllocateDescriptorSets(renderstate.device, &set_alloc_info, &renderstate.heap.global_set);
    }

    // Create samplers then write them to descriptor set.
    bindless_heap_create_all_samplers();
    {
        VkDescriptorImageInfo sampler_infos[FG_SAMPLER_COUNT];
        for (uint32_t i = 0; i < FG_SAMPLER_COUNT; ++i)
        {
            sampler_infos[i] = (VkDescriptorImageInfo){
                .sampler = renderstate.heap.samplers[i]
            };
        }

        VkWriteDescriptorSet descriptor_write = {
            .sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext             = NULL,
            .dstSet            = renderstate.heap.global_set,
            .dstBinding        = 1,  // <- Samplers at binding=1
            .dstArrayElement   = 0,
            .descriptorCount   = FG_SAMPLER_COUNT,
            .descriptorType    = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo        = sampler_infos,
            .pBufferInfo       = NULL,
            .pTexelBufferView  = NULL
        };
        vkUpdateDescriptorSets(renderstate.device, 1, &descriptor_write, 0, NULL);
    }
}

void bindless_heap_shutdown()
{
    for (uint32_t i = 0; i < FG_SAMPLER_COUNT; ++i)
    {
        vkDestroySampler(renderstate.device, renderstate.heap.samplers[i], NULL);
    }
    vkDestroyDescriptorPool(renderstate.device, renderstate.heap.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(renderstate.device, renderstate.heap.set_layout, NULL);
}

void bindless_heap_create_all_samplers()
{
    for (uint32_t i = 0; i < FG_SAMPLER_COUNT; ++i)
    {
        VkSamplerCreateInfo sampler_create_info;

        FG_SamplerType type = (FG_SamplerType)i;
        switch (type)
        {
            case FG_SAMPLER_NEAREST_REPEAT:
                sampler_create_info = (VkSamplerCreateInfo){
                    .sType             = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .pNext             = NULL,
                    .flags             = 0,
                    .magFilter         = VK_FILTER_NEAREST,
                    .minFilter         = VK_FILTER_NEAREST,
                    .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .mipLodBias        = 0.0f,
                    .anisotropyEnable  = VK_FALSE,
                    .maxAnisotropy     = 0.0f,
                    .compareEnable     = VK_FALSE,
                    .compareOp         = VK_COMPARE_OP_NEVER,
                    .minLod            = 0.0f,
                    .maxLod            = VK_LOD_CLAMP_NONE,
                    .borderColor       = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    .unnormalizedCoordinates = VK_FALSE
                };
                break;
            
            case FG_SAMPLER_LINEAR_REPEAT:
                sampler_create_info = (VkSamplerCreateInfo){
                    .sType             = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .pNext             = NULL,
                    .flags             = 0,
                    .magFilter         = VK_FILTER_LINEAR,
                    .minFilter         = VK_FILTER_LINEAR,
                    .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .mipLodBias        = 0.0f,
                    .anisotropyEnable  = VK_FALSE,
                    .maxAnisotropy     = 0.0f,
                    .compareEnable     = VK_FALSE,
                    .compareOp         = VK_COMPARE_OP_NEVER,
                    .minLod            = 0.0f,
                    .maxLod            = VK_LOD_CLAMP_NONE,
                    .borderColor       = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    .unnormalizedCoordinates = VK_FALSE
                };
                break;

            case FG_SAMPLER_ANISOTROPIC_REPEAT:
                sampler_create_info = (VkSamplerCreateInfo){
                    .sType             = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .pNext             = NULL,
                    .flags             = 0,
                    .magFilter         = VK_FILTER_LINEAR,
                    .minFilter         = VK_FILTER_LINEAR,
                    .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .mipLodBias        = 0.0f,
                    .anisotropyEnable  = VK_TRUE,
                    // TODO: Could let user set anisotropy instead of using the max. (i.e. add to config file)
                    .maxAnisotropy     = renderstate.physical_device_properties.limits.maxSamplerAnisotropy,
                    .compareEnable     = VK_FALSE,
                    .compareOp         = VK_COMPARE_OP_NEVER,
                    .minLod            = 0.0f,
                    .maxLod            = VK_LOD_CLAMP_NONE,
                    .borderColor       = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    .unnormalizedCoordinates = VK_FALSE
                };
                break;
            
            case FG_SAMPLER_SHADOW:
                sampler_create_info = (VkSamplerCreateInfo){
                    .sType             = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .pNext             = NULL,
                    .flags             = 0,
                    .magFilter         = VK_FILTER_LINEAR,
                    .minFilter         = VK_FILTER_LINEAR,

                    // Must clamp to border opaque white so that stuff outside the shadow map is considered unshadowed.
                    .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .mipLodBias        = 0.0f,
                    .anisotropyEnable  = VK_FALSE,
                    .maxAnisotropy     = 0,

                    // Hardware shadow comparison
                    .compareEnable     = VK_TRUE,
                    .compareOp         = VK_COMPARE_OP_LESS_OR_EQUAL,  // LESS because not using reverse-z depth

                    .minLod            = 0.0f,
                    .maxLod            = VK_LOD_CLAMP_NONE,
                    .borderColor       = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                    .unnormalizedCoordinates = VK_FALSE
                };
                break;

            default:
                SDL_assert(0 && "Unhandled sampler type.");
        }

        VK_CHECK(vkCreateSampler(renderstate.device, &sampler_create_info, NULL, &renderstate.heap.samplers[i]));
    }
}

