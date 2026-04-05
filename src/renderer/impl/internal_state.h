#ifndef ENGINE_RENDERER_RENDER_STATE_H
#define ENGINE_RENDERER_RENDER_STATE_H

#include "renderer.h"
#include "internal_structs.h"
#include "framegraph.h"

#define PIPELINE_HASING_IMPLEMENTATION
#include "pipeline_keying.h"

#include "shaders.h"
#include "drawcall.h"
#include "mapped_linear_allocator.h"
#include "renderpasses/metadata.h"
#include "game_resources.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

typedef struct RenderState
{
    ThreadData main;  // main.tt is the main thread allocation tracker for the renderer.

    SDL_Window* window;
    b32 using_validation_layers;
    b32 program_caused_vulkan_validation_layer_errors;
    u64 frame_number;

    Renderer_Settings settings;
    VkSampleCountFlagBits multisampling_count_flag;  // NOTE: Vulkan version of msaa setting stored outside settings struct to avoid exposing Vulkan API to game module


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

    // Pipeline Keying system and shader registry
    // FUTURE: If multithreading drawcalls are needed, see this article on sharing pipelines while using seperate maps per thread:
    //         https://ruby0x1.github.io/machinery_blog_archive/post/vulkan-pipelines-and-render-states/index.html    
    PipelineEntry* pipeline_map;          // Recreated only when swapchain format changes (so never under most circumstances)
    ShaderRegistry shader_registry;
    ResourceIDs rids;  // IDs into registry, framegraph, or pipeline hash

    // Per Frame Table for converting Pass Type into framegraph.passes array index
    uint32_t pass_id_from_type[PASS_TYPE_COUNT];  // ONLY use after framegraph has been build that frame

    // Arenas reset each frame
    MappedArena object_transforms;
    MappedArena joint_transforms;

    // Draw Calls are accumulated each frame per shader
    DrawCallsPerShader drawcalls_collection;
    glm::mat4 camera_view;
    glm::mat4 fullscreen_proj;

    // Renderer execution state:
    VkPipeline currently_bound_pipeline;  // Used to avoid  vkCmdBindPipeline call if it's already bound

    // Scene Assets
    b32 is_next_scene_set;
    Scene_InitInfo next_scene_info;

    // Renderables arena
    RenderView renderables_arena;

    // imgui descriptor pool
    VkDescriptorPool imgui_descriptor_pool;

    // ImGui game-side UI callback
    Renderer_ImGuiBuildCallback imgui_callback      = nullptr;
    void*                       imgui_callback_data = nullptr;
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

#endif  // ENGINE_RENDERER_RENDER_STATE_H
