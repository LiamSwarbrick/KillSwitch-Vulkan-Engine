#ifndef ENGINE_RENDERER_RENDER_STATE_H
#define ENGINE_RENDERER_RENDER_STATE_H

#include "../renderer/renderer.h"
#include "internal_structs.h"
#include "framegraph.h"

#include "pipeline_keying.h"

#include "shaders.h"
#include "drawcall.h"
#include "mapped_linear_allocator.h"
#include "renderpasses/metadata.h"
#include "game_resources.h"

#include "debug_ui_api.h"
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
    VkImageUsageFlags swapchain_usage;
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
    //         (if this link dies, I have forked the blog myself)
    PipelineEntry* pipeline_map;          // Recreated only when swapchain format changes (so never under most circumstances)
    ShaderRegistry shader_registry;
    ResourceIDs rids;  // IDs into registry, framegraph, or pipeline hash


    // Arenas that reset each frame
    MappedArena scenes_arena;
    MappedArena object_transforms;
    MappedArena joint_transforms;
    
    RenderView renderables_arena;  // Renderables array which gets sorted into draw calls by which shaders they use
    DrawCallsPerShader drawcalls_collection;  // Draw Calls are accumulated each frame per shader

    // Before shadowing a spotlight, we check if it's already been shadowed
    // Done by hashing the spotlight data relevant to the shadowing
    // NOTE: When doing the shadow map, we can use the radius to set the far plane optimally
    // FUTURE: Render shadow map into a texture atlas so multiple go in one.
    //         That allows dynamically sized shadows (e.g. small spotlights don't take up a whole texture)
    //         since all shadow map textures should be preallocated each frame.
    #define MAX_SHADOWED_SPOTLIGHTS 1    // <- ONE FOR NOW TIL THINGS ARE WORKING
    uint32_t num_shadowed_spotlights;
    uint32_t currently_shadowed_spotlight_indices[MAX_SHADOWED_SPOTLIGHTS];

    CameraInfo main_camera;

    // Renderer execution state:
    VkPipeline currently_bound_pipeline;  // Used to avoid  vkCmdBindPipeline call if it's already bound

    // Scene Assets
    b32 is_next_scene_set;
    Scene_InitInfo next_scene_info;




    // IMGUI
    //

    // imgui descriptor pool
    VkDescriptorPool imgui_descriptor_pool;

    // ImGui game-side UI callback
    DebugUI_ImGuiBuildCallback  imgui_callback      = nullptr;
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
