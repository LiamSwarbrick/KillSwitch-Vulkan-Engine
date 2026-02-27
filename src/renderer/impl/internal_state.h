#ifndef ENGINE_RENDERER_RENDER_STATE_H
#define ENGINE_RENDERER_RENDER_STATE_H

#include "../renderer.h"

#include "internal_structs.h"
#include "framegraph.h"
// #include "due_rework/internals_due_rework.h"

typedef struct ThreadData
{
    // Thread Tracker (provides memory leak checking). NOTE: Make sure all CPU allocations use L_calloc() and L_free().
    ThreadAllocTracker tt;
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

typedef struct RenderState
{
    // Main thread:
    ThreadData main;

    SDL_Window* window;
    b32 using_validation_layers;
    b32 program_caused_vulkan_validation_layer_errors;
    u64 frame_number;

    b32 uncapped_fps;

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

    // Swapchain
    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    #define MAX_SWAPCHAIN_IMAGE_COUNT 10
    u32 swapchain_image_count;
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkSemaphore swapchain_image_rendering_complete_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];

    // Multiple frames in flight to avoid stalling pipeline between frames
#define NUM_FRAMES_IN_FLIGHT 2
    FrameState frames[NUM_FRAMES_IN_FLIGHT];

    // FrameGraph
    ResourceRegistry registry;
    BindlessHeap heap;

    // The old stuff that I want to redo, but first need something up on the screen for others to work from.
    // E.g. Get cube rendering running, and then people can work on input and player movement
    // Implementing collisions with a physics engine (jolt)
    // OldRenderState old;
}
RenderState;

extern RenderState renderstate;  // Declared in renderer.cpp

// // In due_rework/:
// void old_stuff_init(RenderState* renderstate);
// void old_stuff_clean(RenderState* renderstate);
// void old_create_swapchain_tied_objects(RenderState* renderstate, VkFormat old_format);
// void old_destroy_swapchain_tied_objects(RenderState* renderstate);
// //
// const char* get_render_mode_name(RenderMode render_mode);
// VkCommandBuffer  begin_one_time_command(RenderState* renderstate);
// void             end_one_time_command_and_wait(RenderState* renderstate, VkCommandBuffer command);
// GPU_Image        create_image_texture2d(RenderState* renderstate, u8* data, u64 data_size, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage);
// GPU_Image        create_render_target_attachment(RenderState* renderstate, b32 is_depth_attachment, VkFormat desired_format, VkExtent3D extent, VkImageUsageFlags usage);
// GraphicsPipeline create_graphics_pipeline(RenderState* renderstate, GraphicsPipelineConfigInfo config);
// void             destroy_graphics_pipeline(RenderState* renderstate, GraphicsPipeline* gp);
// void             create_all_graphics_pipelines(RenderState* renderstate);
// void             destroy_all_graphics_pipelines(RenderState* renderstate);
// // (end of due rework)

// Not in due_rework/:

void _Renderer_OnWindowResize();
void _Renderer_OnWindowMinimize();

QueueFamilyIndices      get_physical_device_queue_family_indices(VkPhysicalDevice physical_device);
int                     score_physical_device_and_check_required_features(VkPhysicalDevice physical_device);  // Negative score means unsuitable device
SwapChainSupportDetails get_and_alloc_swap_chain_support_details(VkPhysicalDevice physical_device);
void                    free_swap_chain_support_details(SwapChainSupportDetails details);

void create_or_recreate_swapchain();
void destroy_swapchain();

// GPU_Buffer create_buffer(VmaAllocator vma_allocator, u64 size, VkBufferUsageFlags buffer_usage_flags, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage);
// void       destroy_buffer(VmaAllocator vma_allocator, const GPU_Buffer* gpu_buffer);
// GPU_Buffer create_staging_buffer_from_data(VmaAllocator vma_allocator, u8* data, u64 size);
// void       destroy_image(VkDevice device, VmaAllocator vma_allocator, GPU_Image gpu_image);
// u32        compute_num_mip_levels(u32 image_level0_width, u32 image_level0_height);
// GPU_Image  create_attachment_image(RenderState* renderstate, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect_flags, b32 has_msaa);

#endif  // ENGINE_RENDERER_RENDER_STATE_H
