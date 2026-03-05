#ifndef ENGINE_RENDERER_RENDER_STATE_H
#define ENGINE_RENDERER_RENDER_STATE_H

#include "../renderer.h"

#include "internal_structs.h"
#include "framegraph.h"
#include "pass_definitions.h"
// #include "due_rework/internals_due_rework.h"

typedef struct RenderState
{
    ThreadData main;  // main.tt is the main thread allocation tracker for the renderer.

    SDL_Window* window;
    b32 using_validation_layers;
    b32 program_caused_vulkan_validation_layer_errors;
    u64 frame_number;

    // Options
    // TODO: Instead, just ask core for settings when needed.
    // Or realistically, each module can have a Settings_Changed callback
    // Since swapchain will need to be recreated if uncapped_fps is changed.
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
    VkQueue transfer_queue;
    SDL_Mutex* transfer_queue_mutex;  // Multithreaded submission onto a queue must be synchronised.

    // Swapchain
    VkSwapchainKHR swapchain;
    VkFormat    swapchain_image_format;
    VkExtent2D  swapchain_extent;
    u32         swapchain_image_count;
    VkImage     swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkSemaphore swapchain_image_rendering_complete_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];

    // Multiple frames in flight to avoid stalling pipeline between frames
    FrameState frames[NUM_FRAMES_IN_FLIGHT];

    // FrameGraph
    FrameGraph        framegraph;
    ResourceRegistry  registry;
    BindlessHeap      heap;
    VkPipelineLayout  global_pipeline_layout;

    // IDs into registry, framegraph, or pipeline hash
    ResourceIDs rids;          // Recreated when window/swapchain resizes
    PassIDs     pass_ids;      // Recreated each frame
    PipelineIDs pipeline_ids;  // Recreated only when swapchain format changes (so almost never)

    // Pipelines: Created lazily, cleaned when swapchain changes format (and remade lazily the next frame).
    // TODO:...
    VkPipeline temp_pipeline;
}
RenderState;

extern RenderState renderstate;  // Declared in renderer.cpp

void _Renderer_OnWindowResize();
void _Renderer_OnWindowMinimize();

QueueFamilyIndices      get_physical_device_queue_family_indices(VkPhysicalDevice physical_device);
int                     score_physical_device_and_check_required_features(VkPhysicalDevice physical_device);  // Negative score means unsuitable device
SwapChainSupportDetails get_and_alloc_swap_chain_support_details(VkPhysicalDevice physical_device);
void                    free_swap_chain_support_details(SwapChainSupportDetails details);

void create_or_recreate_swapchain();
void destroy_swapchain();

void create_thread_staging_objects(ThreadStagingObjects* staging_objects);
void destroy_thread_staging_objects(ThreadStagingObjects* staging_objects);
void thread_safe_submit_cmd(VkCommandBuffer cmd, VkFence fence);

// GPU_Buffer create_buffer(VmaAllocator vma_allocator, u64 size, VkBufferUsageFlags buffer_usage_flags, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage);
// void       destroy_buffer(VmaAllocator vma_allocator, const GPU_Buffer* gpu_buffer);
// GPU_Buffer create_staging_buffer_from_data(VmaAllocator vma_allocator, u8* data, u64 size);
// void       destroy_image(VkDevice device, VmaAllocator vma_allocator, GPU_Image gpu_image);
// u32        compute_num_mip_levels(u32 image_level0_width, u32 image_level0_height);
// GPU_Image  create_attachment_image(RenderState* renderstate, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect_flags, b32 has_msaa);

#endif  // ENGINE_RENDERER_RENDER_STATE_H
