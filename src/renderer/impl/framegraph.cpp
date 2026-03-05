#include "framegraph.h"

#include "internal_state.h"

// Init Subsystem
//

void FG_Init()
{
    FG_BindlessHeap_Init();
    
    // Create global pipeline layout that uses this heap
    {
        // Push constant range of 128 bytes should be safe across all hardware (Intel/Nvidia/AMD)
        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = 128
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
    FG_BindlessHeap_Shutdown();
    vkDestroyPipelineLayout(renderstate.device, renderstate.global_pipeline_layout, NULL);
}

// Graph Building
//

uint32_t FG_AddPass(RenderPassDesc pass_description)
{
    printf("DEBUG: Adding pass %s\n", pass_description.debug_name);

    FrameGraph* fg = &renderstate.framegraph;
    SDL_assert(fg->pass_count < MAX_PASSES);

    uint32_t pass_id = fg->pass_count++;
    RenderPassDesc* pass = &fg->passes[pass_id];
    memcpy(pass, &pass_description, sizeof(RenderPassDesc));

    return pass_id;
}

// Graph Execution
//

void FG_ApplyBarriers(VkCommandBuffer cmd, RenderPassDesc* pass)
{
    VkImageMemoryBarrier2 image_barriers[MAX_PASS_RESOURCE_BANDWIDTH];
    uint32_t img_count = 0;
    
    VkBufferMemoryBarrier2 buffer_barriers[MAX_PASS_RESOURCE_BANDWIDTH];
    uint32_t buf_count = 0;

    // Analyse outputs (or inputs) to see what needs transitioning
    for (uint32_t i = 0; i < pass->output_count; i++)
    {
        PassResourceUsage* usage = &pass->outputs[i];
        FG_Resource* res = &renderstate.registry.resources[usage->id];

        if (res->type == FG_RESOURCE_TYPE_IMAGE)
        {
            image_barriers[img_count++] = (VkImageMemoryBarrier2){
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext               = NULL,
                .srcStageMask        = res->current_stage,
                .srcAccessMask       = res->current_access,
                .dstStageMask        = usage->stage,
                .dstAccessMask       = usage->access,
                .oldLayout           = res->current_layout,
                .newLayout           = usage->layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = res->image.handle,
                .subresourceRange    = {
                    .aspectMask       = res->image.subresource_range.aspectMask,
                    .baseMipLevel     = 0,
                    .levelCount       = 1,
                    .baseArrayLayer   = 0,
                    .layerCount       = 1
                }
            };
        } 
        else if (res->type == FG_RESOURCE_TYPE_BUFFER)
        {
            buffer_barriers[buf_count++] = (VkBufferMemoryBarrier2){
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

        // Update the resource sync state post-transition 
        //

        // Both types share these sync fields
        res->current_access = usage->access;
        res->current_stage  = usage->stage;

        // Only images care about layout
        if (res->type == FG_RESOURCE_TYPE_IMAGE)
        {
            res->current_layout = usage->layout;
        }
    }

    SDL_assert(img_count < sizeof(image_barriers) / sizeof(image_barriers[0]));
    SDL_assert(buf_count < sizeof(buffer_barriers) / sizeof(buffer_barriers[0]));

    // One single call to synchronize everything for this pass
    VkDependencyInfo dep = {
        .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                     = NULL,

        // VK_DEPENDENCY_BY_REGION_BIT is used on tiled GPUs for example.
        // E.g. the barrier will be local to each tile instead of stalling the whole pipeline.
        .dependencyFlags           = VK_DEPENDENCY_BY_REGION_BIT,

        .memoryBarrierCount        = 0,
        .pMemoryBarriers           = NULL,
        .bufferMemoryBarrierCount  = buf_count,
        .pBufferMemoryBarriers     = buffer_barriers,
        .imageMemoryBarrierCount   = img_count, 
        .pImageMemoryBarriers      = image_barriers
    };
    vkCmdPipelineBarrier2(cmd, &dep);
}

void FG_ExecutePass(uint32_t pass_idx, VkCommandBuffer cmd)
{
    FrameGraph* fg = &renderstate.framegraph;
    RenderPassDesc* pass = &fg->passes[pass_idx];
    FG_ApplyBarriers(cmd, pass);
    
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

            SDL_assert(usage->id < renderstate.registry.resource_count);
            FG_Resource* res = &renderstate.registry.resources[usage->id];

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
    FrameGraph* fg = &renderstate.framegraph;
    SDL_assert(fg->pass_count < MAX_PASSES);

    for (uint32_t i = 0; i < fg->pass_count; ++i)
    {
        FG_ExecutePass(i, cmd);
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

typedef struct NewResourceInfo
{
    ResourceImportInfo import_info;
    VmaAllocation allocation;  // For created resources. Use VK_NULL_HANDLE for imported resources.
}
NewResourceInfo;

uint32_t add_resource_to_registry_and_heap(const char* debug_name, FG_ResourceType type, NewResourceInfo resource_info)
{
    SDL_assert(renderstate.registry.resource_count < MAX_RESOURCES);

    uint32_t id = renderstate.registry.resource_count++;
    FG_Resource* res = &renderstate.registry.resources[id];

    // For safety, zero the struct, but this function should manully set all parameters.
    memset(res, 0, sizeof(FG_Resource));

    // Set shared fields
    strncpy(res->debug_name, debug_name, sizeof(res->debug_name));
    res->type = type;
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
            res->image_bindless_index = UINT32_MAX;
        }
    }

    // Returns the resource's id into the registry
    return id;
}

uint32_t FG_CreateResource(const char* debug_name, FG_ResourceType type, ResourceCreateInfo* create_info)
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
    return add_resource_to_registry_and_heap(debug_name, type, resource_info);
}

uint32_t FG_ImportResource(const char* debug_name, FG_ResourceType type, ResourceImportInfo import_info)
{
    NewResourceInfo resource_info = {
        .import_info = import_info,
        .allocation = VK_NULL_HANDLE
    };

    // Returns the resource's id into the registry
    return add_resource_to_registry_and_heap(debug_name, type, resource_info);
}

void FG_BindlessHeap_Init()
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
    FG_BindlessHeap_CreateAllSamplers();
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

void FG_BindlessHeap_Shutdown()
{
    for (uint32_t i = 0; i < FG_SAMPLER_COUNT; ++i)
    {
        vkDestroySampler(renderstate.device, renderstate.heap.samplers[i], NULL);
    }
    vkDestroyDescriptorPool(renderstate.device, renderstate.heap.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(renderstate.device, renderstate.heap.set_layout, NULL);
}

void FG_BindlessHeap_CreateAllSamplers()
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

void FG_ClearResources()
{
    vkDeviceWaitIdle(renderstate.device);

    for (uint32_t i = 0; i < renderstate.registry.resource_count; ++i)
    {
        FG_Resource* res = &renderstate.registry.resources[i];

        // Imported resources won't have a VMA allocation
        if (res->allocation == VK_NULL_HANDLE)
        {
            continue;  // Imported resources are managed by whoever imported them.
        }

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

        // Wipe the struct memory to prevent accidental reuse
        memset(res, 0, sizeof(FG_Resource));
    }

    // Reset counters so the next "Create" call starts from index 0
    renderstate.registry.resource_count = 0;
    renderstate.heap.texture_count = 0;
    
    // Note: We don't destroy the global_set or samplers here because 
    // those live for the lifetime of the engine (FG_Init / FG_Shutdown).
    
    SDL_Log("FrameGraph Resources Destroyed & Registry Reset.");
}
