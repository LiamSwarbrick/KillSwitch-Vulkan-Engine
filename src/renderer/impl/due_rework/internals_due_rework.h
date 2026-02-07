#ifndef RENDERER_INTERNALS_DUE_REWORK_H
#define RENDERER_INTERNALS_DUE_REWORK_H

#include "../vulkan_wrapper.h"
#include "core/my_c_runtime.h"
#include "internal_structs.h"
#include "shared_glsl_defs.h"

typedef struct OldFrameData
{
    // Persistant until program end
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    VkFence render_fence;
    // VkSemaphore swapchain_image_acquired_semaphore;

    // Lights Buffer (double buffered since per frame CPU to GPU copying is involved)
    GPU_Buffer lights_buffer;
    VkDescriptorSet lights_descriptor_set;  // Points to lights_buffer
}
OldFrameData;

typedef struct OldRenderState
{
    // Double buffer our command buffers so that while one is being executed
    // by the GPU, we still have access to another set to write the next commands to.
    #define FRAME_OVERLAP 2
    u32 current_frame_id;  // Index of frames we write to this frame
    OldFrameData frames[FRAME_OVERLAP];

    // Draw resources
    VkExtent2D render_target_extent;
    GPU_Image render_target_image;
    GPU_Image render_target_pingpong_image;  // Used for chaining post processing effects: e.g. draw scene to render_target, then apply with bloom to pingpong, (etc..) then finally render to swapchain
    GPU_Image render_target_depth;
    union
    {
        struct
        {
            GPU_Image render_target_deferred_albedo_roughness;  // rgb:albedo,   a:roughness
            GPU_Image render_target_deferred_normal_metalness;  // rgb:normal,   m:metalness
            GPU_Image render_target_deferred_emissive_ao;       // rgb:emissive, a:ambient occlusion
        };
        GPU_Image render_target_deferred_attachments[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT];
    };
    GPU_Image shadow_map_depth;  // TODO: Implement more than just one shadow map. Also cascades, capsules? etc.

    VkExtent2D bloom_target_extent;
    GPU_Image bloom_target_image;
    GPU_Image bloom_pingpong_image;  // We apply a vertical and horizontal blur in two passes, so we need two targets so that we can swap between reading from one, and writing to the other.


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

    // General pool to allocate descriptor sets from
    VkDescriptorPool descriptor_pool;

    // Descriptor set layouts (defines how descriptor sets bind to shader code)
    VkDescriptorSetLayout scene_set_layout;
    VkDescriptorSetLayout object_set_layout;
    VkDescriptorSetLayout postprocess_set_layout;
    VkDescriptorSetLayout gbuffers_set_layout;
    VkDescriptorSetLayout lights_set_layout;
    VkDescriptorSetLayout shadow_maps_set_layout;
    VkDescriptorSetLayout bloom_apply_set_layout;

    GPU_Buffer      scene_uniform_buffer;
    VkDescriptorSet scene_descriptor_set;

    VkSampler       postprocess_sampler;
    VkDescriptorSet postprocess_descriptor_set;
    VkDescriptorSet postprocess_pingpong_descriptor_set;
    VkDescriptorSet bloom_target_postprocess_descriptor_set;
    VkDescriptorSet bloom_pingpong_postprocess_descriptor_set;
    VkDescriptorSet bloom_apply_descriptor_set;

    VkDescriptorSet gbuffers_descriptor_set;

    VkSampler       shadow_map_sampler;
    VkDescriptorSet shadow_maps_descriptor_set;

}
OldRenderState;

// Stuff I don't want in my vulkan wrapper anymore:
VkCommandBuffer vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool);
VkCommandBufferSubmitInfo vklayer_command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 vklayer_submit_info(VkCommandBufferSubmitInfo* cmd_submit_info, VkSemaphoreSubmitInfo* signal_semaphore_submit_info, VkSemaphoreSubmitInfo* wait_semaphore_submit_info);

VkImageSubresourceRange vklayer_image_subresource_range(VkImageAspectFlags aspect_mask);
VkImageMemoryBarrier2 vklayer_specify_image_transition_barrier(
    VkImage image, VkImageSubresourceRange subimage,

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
);
void vklayer_cmd_transition_images(VkCommandBuffer cmd, u32 image_barrier_count, const VkImageMemoryBarrier2* image_barriers);
VkBufferMemoryBarrier2 vklayer_specify_buffer_barrier(
    VkBuffer buffer, VkDeviceSize buffer_size, VkDeviceSize buffer_offset,
    
    // Before barrirer
    VkPipelineStageFlags2 current_pipeline_stage,
    VkAccessFlags2        current_access_flags,
    u32                   current_queue_family_index,

    // After barrier:
    VkPipelineStageFlags2 new_pipeline_stage,
    VkAccessFlags2        new_access_flags,
    u32                   new_queue_family_index
);
void vklayer_cmd_pipeline_barrier_for_buffers(VkCommandBuffer cmd, u32 barrier_count, const VkBufferMemoryBarrier2* barriers);


#endif  // RENDERER_INTERNALS_DUE_REWORK_H
