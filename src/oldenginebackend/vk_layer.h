#ifndef MY_VK_LAYER_H
#define MY_VK_LAYER_H

// NOTE: VOLK_IMPLEMENTATION is #defined in vk_layer.c
// and mustn't be defined in more than one compilation unit.
#include <volk/volk.h>
#include <vk_mem_alloc.h>

#include "my_c_runtime.h"

const char* vklayer_result_to_string(VkResult result);
void vklayer_print_queueflagbits(VkQueueFlagBits flags);
void vklayer_print_memoryheapflagbits(VkMemoryHeapFlags flags);
void vklayer_print_memorypropertyflagbits(VkMemoryPropertyFlags flags);

#define VK_CHECK(x)                                                         \
do                                                                          \
{                                                                           \
    VkResult err = (x);                                                     \
    if (err != VK_SUCCESS)                                                  \
    {                                                                       \
        fprintf(stderr, "[%s:%d] Vulkan error: %s (%d)\n",                  \
            __FILE__, __LINE__, vklayer_result_to_string(err), (int)(err)); \
        assert(0 && "VK_CHECK() not successful.");                          \
        abort();                                                            \
    }                                                                       \
} while (0)

// Sync timeouts specifically for swapchain images:
// because this should be instant.
#ifdef NDEBUG
    // Wait forever in release mode
    #define SYNC_TIMEOUT_NANOSECONDS  UINT64_MAX
#else
    // Only wait one second in debug mode to detect deadlocks/hangs
    #define SYNC_TIMEOUT_NANOSECONDS  1000000000UL
#endif



///


VkCommandBuffer vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool);

//
// Helper functions for pipeline barriers
//

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


// Blitting
void vklayer_cmd_blit_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage dest, VkExtent2D src_size, VkExtent2D dst_size);


//
// Synchronisation Helper functions
//

VkSemaphoreSubmitInfo vklayer_semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo vklayer_command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 vklayer_submit_info(VkCommandBufferSubmitInfo* cmd_submit_info, VkSemaphoreSubmitInfo* signal_semaphore_submit_info, VkSemaphoreSubmitInfo* wait_semaphore_info);

//
// Image helper functions
//

VkFormat vklayer_find_supported_depth_format(VkPhysicalDevice physical_device);
u32 compute_num_mip_levels(u32 image_level0_width, u32 image_level0_height);

// TODO: Deprecate these two:
VkImageCreateInfo vklayer_basic_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);
VkImageViewCreateInfo vklayer_basic_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);



#endif  // MY_VK_LAYER_H
