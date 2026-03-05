#ifndef ENGINE_RENDERER_INTERNAL_STRUCTS_H
#define ENGINE_RENDERER_INTERNAL_STRUCTS_H

// NOTE: Must not include internal_state.h
#include "../renderer.h"
#include "vulkan_wrapper.h"

#define MAX_SWAPCHAIN_IMAGE_COUNT 10
#define NUM_FRAMES_IN_FLIGHT 2


typedef struct ThreadStagingObjects
{
    // Command pool with upload buffer for copying assets from staging buffers
    VkCommandPool transfer_command_pool;
    VkCommandBuffer upload_command_buffer;
}
ThreadStagingObjects;

typedef struct ThreadData
{
    // Thread Tracker (provides memory leak checking). NOTE: Make sure all CPU allocations use L_calloc() and L_free().
    ThreadAllocTracker tt;

    ThreadStagingObjects staging_objects;
}
ThreadData;

typedef struct FrameState
{
    VkFence rendering_complete_fence;
    VkSemaphore swapchain_image_acquired_semaphore;
    VkCommandPool graphics_command_pool;
    VkCommandBuffer graphics_command_buffer;
}
FrameState;

// This enum exists so NUM_QUEUE_FAMILIES stores the correct value
enum _Enum_QueueFamilyIndices
{
    QUEUE_GRAPHICS_FAMILY=0,
    QUEUE_PRESENT_FAMILY,
    QUEUE_TRANSFER_FAMILY,
    
    NUM_QUEUE_FAMILY_INDICES
};

// Indices for which vulkan queue family stores each type of queue we use
typedef union QueueFamilyIndices
{
    // Same order as enum
    struct
    {
        u32 graphics_family;
        u32 present_family;
        u32 transfer_family;
    };
    u32 array[NUM_QUEUE_FAMILY_INDICES];
}
QueueFamilyIndices;

typedef struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    u32 present_mode_count;
    VkSurfaceFormatKHR* formats;
    VkPresentModeKHR* present_modes;
}
SwapChainSupportDetails;

// typedef struct GPU_Buffer
// {
//     VkBuffer buffer;
//     VmaAllocation allocation;
//     VmaAllocationInfo info;
// }
// GPU_Buffer;

// typedef struct GPU_Image
// {
//     VkImageLayout   current_layout;
//     VkImage         image;
//     VkImageView     image_view;
//     VmaAllocation   allocation;
//     VkExtent3D      image_extent;
//     VkFormat        image_format;
// }
// GPU_Image;

// typedef struct SPIRVConfig
// {
//     const char* spirv_path;
//     const char* entrypoint_name;
//     const VkSpecializationInfo* pSpecializationInfo;
// }
// SPIRVConfig;

#endif  // ENGINE_RENDERER_INTERNAL_STRUCTS_H
