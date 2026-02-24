#include "framegraph.h"

#include "internal_state.h"

void update_resource_tracker(uint32_t id, VkImageLayout new_layout, VkAccessFlags2 new_access, VkPipelineStageFlags2 new_stage)
{
    FG_Resource* res = &renderstate.registry.resources[id];
    
    // Both types share these sync fields
    res->last_access = new_access;
    res->last_stage  = new_stage;

    // Only images care about layout
    if (res->type == FG_RESOURCE_TYPE_IMAGE)
    {
        res->image.last_layout = new_layout;
    }
}

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
                .srcStageMask        = res->last_stage,
                .srcAccessMask       = res->last_access,
                .dstStageMask        = usage->stage,
                .dstAccessMask       = usage->access,
                .oldLayout           = res->image.last_layout,
                .newLayout           = usage->layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = res->image.handle,
                .subresourceRange    = {
                    .aspectMask       = res->image.aspect_mask,
                    .baseMipLevel     = 0,
                    .levelCount       = 1,
                    .baseArrayLayer   = 0,
                    .layerCount       = 1,
                }
            };
        } 
        else if (res->type == FG_RESOURCE_TYPE_BUFFER)
        {
            buffer_barriers[buf_count++] = (VkBufferMemoryBarrier2){
                .sType                = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext                = NULL,
                .srcStageMask         = res->last_stage,
                .srcAccessMask        = res->last_access,
                .dstStageMask         = usage->stage,
                .dstAccessMask        = usage->access,
                .srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
                .buffer               = res->buffer.handle,
                .offset               = 0,
                .size                 = res->buffer.size
            };
        }

        // Update the "Truth" for the next pass
        update_resource_tracker(usage->id, usage->layout, usage->access, usage->stage);
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

void FG_ExecutePass(FrameGraph* fg, uint32_t pass_idx, VkCommandBuffer cmd)
{
    RenderPassDesc* pass = &fg->passes[pass_idx];
    FG_ApplyBarriers(cmd, pass);
    
    if (pass->is_compute)
    {
        // Compute passes don't use vkCmdBeginRendering
        pass->execute_callback(cmd, pass->user_data);
    }
    else
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
