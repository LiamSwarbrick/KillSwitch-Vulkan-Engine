#ifndef ENGINE_RENDERER_INTERNAL_STRUCTS_H
#define ENGINE_RENDERER_INTERNAL_STRUCTS_H

// NOTE: Must not include internal_state.h
#include "../renderer.h"
#include "vulkan_wrapper.h"

// This enum exists so NUM_QUEUE_FAMILIES stores the correct value
enum _Enum_QueueFamilyIndices
{
    QUEUE_GRAPHICS_FAMILY=0,
    QUEUE_PRESENT_FAMILY,
    
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
