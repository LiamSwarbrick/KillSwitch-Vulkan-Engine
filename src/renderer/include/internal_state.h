#ifndef ENGINE_RENDERER_RENDER_STATE_H
#define ENGINE_RENDERER_RENDER_STATE_H

#include "../renderer.h"

#include "internal_structs.h"
#include "internals_due_rework.h"

typedef struct ThreadData
{
    // Thread Tracker (provides memory leak checking). NOTE: Make sure all CPU allocations use L_calloc() and L_free().
    ThreadAllocTracker tt;
}
ThreadData;

typedef struct RenderState
{
    // Main thread:
    ThreadData main;

    SDL_Window* window;
    b32 using_validation_layers;
    b32 program_caused_vulkan_validation_layer_errors;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    QueueFamilyIndices queue_family_indices;
    VkDevice device;
    VmaAllocator vma_allocator;

    // Queue handles to VkDevice queues (cleaned up automatically when VkDevice is destroyed)
    VkQueue graphics_queue;
    VkQueue presentation_queue;

    OldRenderState old;
}
RenderState;

void old_stuff_init(RenderState* renderstate);
void old_stuff_clean(RenderState* renderstate);

SwapChainSupportDetails get_and_alloc_swap_chain_support_details(VkPhysicalDevice physical_device);
void free_swap_chain_support_details(SwapChainSupportDetails details, ThreadAllocTracker* alloc_tracker);
QueueFamilyIndices get_physical_device_queue_family_indices(VkPhysicalDevice physical_device);
int score_physical_device_and_check_required_features(VkPhysicalDevice physical_device);  // Negative score means unsuitable device

#endif  // ENGINE_RENDERER_RENDER_STATE_H
