#ifndef ENGINE_RENDERER_INTERNAL_STRUCTS_H
#define ENGINE_RENDERER_INTERNAL_STRUCTS_H

// NOTE: Must not include internal_state.h
#include "core/core.h"
#include "vulkan_wrapper.h"
#include "renderer/shadersrc/common/shared.glsl"

#define MAX_SWAPCHAIN_IMAGE_COUNT 10
#define NUM_FRAMES_IN_FLIGHT 5


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

// TODO: I really need a per frame scratch arena instead of all these
typedef struct RenderView
{
    uint32_t num_renderables;
    Renderable* items;
    
    uint32_t num_point_lights;
    uint32_t num_spot_lights;
    PointLight* point_lights;
    SpotLight* spot_lights;
    b32* is_spotlight_shadowed;

    // Clustered shading
    // TODO: Switch to packed array during staging because this is quite big
    uint32_t* staging_point_light_indices;  // CLUSTER_COUNT * MAX_POINTLIGHTS
    uint32_t* staging_spot_light_indices;   // CLUSTER_COUNT * MAX_SPOTLIGHTS
    Cluster*  staging_cluster_offsets;      // CLUSTER_COUNT
}
RenderView;


#endif  // ENGINE_RENDERER_INTERNAL_STRUCTS_H
