//
// TODO: Once I've implemented enough vulkan shit an expressive, careful, OCD-ass rearchitecting pass is 
// imperitive to remove the technical debt that comes with learning a bunch of stuff inside one project.
// Vaguelly I want to have a more general rendergraph thing to turn the long lists of image transitions and 
// renderpass recordings with specific shaders into a simple code-based graph.
// Things like the decimated machinery engine is extremely modular, would be nice to have something super modular,
// or at least have parts of an engine be less intercconnected (like the monolithic VulkanEngine struct)
// Also, was looking at the Haxe Heaps engine and the h3d module looks pretty well organised, maybe take inspiration from that
//
#ifndef L_ENGINE_H
#define L_ENGINE_H

#include "vk_layer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>  // Make sure vulkan is included before glfw

#include "engine_structs.h"
#include "shared_glsl_defs.h"


#define RENDERMODE_LIST(X) \
    X(RENDERMODE_INVALID, "Invalid or uninitialised mode.") \
    \
    X(RENDERMODE_DEFAULT_FORWARD,  "Forward opaque and transparent pass.") \
    X(RENDERMODE_DEFAULT_DEFERRED, "Deferred opaque pass, still forward for transparent pass.") \
    \
    X(RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS,    "Show mipmap levels.") \
    X(RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH,    "Show fragment depth.") \
    X(RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES, "Show derivatives of frag depth.") \
    X(RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW, "Show number of fragment shader invocations per pixel that would occur without depth testing.") \
    X(RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING, "Show number of fragment shader invocations per pixel that occurs with depth testing.") \
    X(RENDERMODE_DEBUGVIZ_MESH_DENSITY,      "Show triangle area per pixel, smaller triangles represent denser meshes.")
// NOTE: X is a macro of the form X(name, desc)
// The description argument is just so we can comment the usecase of each rendermode.
// Macros don't allow comments, so this is the way to do that.
// TODO: If making a controllable rendergraph for a game engine, the description might be a feature we could use.

typedef enum RenderModeEnum
{
    #define AS_ENUM(name, desc) name,
    RENDERMODE_LIST(AS_ENUM)
    #undef AS_ENUM
}
RenderMode;


typedef struct FrameData
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
FrameData;


typedef struct ThreadData
{
    // Thread Tracker (provides memory leak checking). NOTE: Make sure all CPU allocations use L_calloc() and L_free().
    ThreadAllocTracker tt;
}
ThreadData;
// NOTE: CURRENTLY A SINGLE-THREADED APPLICATION
//
// A careful refactor will be required for multithreading.
// Mostly the one time transfer command pool for asset loading, with multithreaded, each worker thread would have a pool,
// and queue submits aren't threadsafe and would need to be synchronised (complicated, which is why I'm not caring about it right now).
// Also can refactor the pools to use a dedicated transfer queue. Currently they use the graphics queue family index

typedef struct Input
{
    b32 key_left;
    b32 key_right;
    b32 key_up;
    b32 key_down;
    b32 key_forward;
    b32 key_back;
    b32 key_move_faster;
    b32 key_move_slower;

    b32 mouse_button_left;
    b32 mouse_button_right;
    b32 mouse_is_captured;
    double mouse_xpos;
    double mouse_ypos;

    b32 key_1;
    b32 key_2;
    b32 key_3;
    b32 key_4;
}
Input;

// NOTE: Useful for error checking: e.g. no swapping scenes if draw is going on, but not used right now.
// typedef enum EngineStagesEnum
// {
//     ENGINE_STAGE_UNINITIALISE=0,
//     ENGINE_STAGE_INIT,
//     ENGINE_STAGE_CLEAN_UP,
//     ENGINE_STAGE_TICK_GAME,
//     ENGINE_STAGE_DRAW,
//     ENGINE_STAGE_INBETWEEN,  // The rest of the mainloop in vk_run
// }
// EngineStages;


// Where the programs run time data goes:
typedef struct VulkanEngine
{
    // EngineStages engine_stage;
    ThreadData main;

    // Current rendering mode: e.g. select PBR or one of the various debug rendering modes
    RenderMode render_mode;
    RenderMode last_render_mode;


    // Video/Windowing
    u64 frame_number;
    VkExtent2D window_extents;
    b32 window_minimized;
    b32 window_resizing;
    b32 uncapped_fps;  // NOTE: Capped uses FIFO_RELAXED_KHR present mode. Uncapped uses MAILBOX_KHR.
    b32 program_caused_vulkan_validation_layer_errors;  // So in debug mode I can have green text saying no validation layer errors on program end.
    GLFWwindow* window;  // TODO: Switch to SDL for a full game because it's an ultacrossplatform API for audio/gamepads/storage/networking etc... (e.g. handles platforms that require certain directories for user data etc.)

    Input input;
    Input last_input;

    b32 using_validation_layers;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    QueueFamilyIndices queue_family_indices;
    VkDevice device;
    VmaAllocator vma_allocator;
    
    // Swapchain
    b32 is_a_swapchain_created;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    u32 swapchain_image_count;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;
    VkSemaphore* swapchain_image_acquired_semaphores;
    VkSemaphore* swapchain_image_render_semaphores;

    // Queue handles to VkDevice queues (cleaned up automatically when VkDevice is destroyed)
    VkQueue graphics_queue;
    VkQueue presentation_queue;

    // Double buffer our command buffers so that while one is being executed
    // by the GPU, we still have access to another set to write the next commands to.
    #define FRAME_OVERLAP 2
    u32 current_frame_id;  // Index of frames we write to this frame
    FrameData frames[FRAME_OVERLAP];

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


    // Graphics Pipeline
    GraphicsPipeline gp_opaque_pass;
    GraphicsPipeline gp_deferred_lighting;
    GraphicsPipeline gp_transparent_pass;
    GraphicsPipeline gp_fullscreen_tri_mosaic;
    GraphicsPipeline gp_shadowmap_opaque;
    GraphicsPipeline gp_shadowmap_transparent;
    GraphicsPipeline gp_bloom_brightness_extraction;
    GraphicsPipeline gp_bloom_gaussian_blur_horizontal;
    GraphicsPipeline gp_bloom_gaussian_blur_vertical;
    GraphicsPipeline gp_bloom_apply;

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

    // Draw Commands per frame get put into buckets based on graphics pipeline
    DrawCommandLists draw_lists;

    Scene scene;
    b32 do_bloom;
    b32 do_postprocessing;

    // Game data: (TODO: Should game data be its own struct in here, and have update_game() be seperate to rendering?)
    float camera_fov;
    glm::vec3 camera_position;
    float camera_pitch;
    float camera_yaw;
    
    glm::vec4 clear_color;
}
VulkanEngine;

void vk_init(VulkanEngine* engine, VkExtent2D window_extents);
void vk_cleanup(VulkanEngine* engine);
void vk_tick_game(VulkanEngine* engine, float dt);
void vk_draw(VulkanEngine* engine);
void vk_dispatch_draws(VulkanEngine* engine);
void vk_run(VulkanEngine* engine);

void glfw_window_size_callback(GLFWwindow* window, int width, int height);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_motion_callback(GLFWwindow* window, double xpos, double ypos);
void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

const char* get_render_mode_name(RenderMode render_mode);

SwapChainSupportDetails get_and_alloc_swap_chain_support_details(VulkanEngine* engine, VkPhysicalDevice physical_device);
void free_swap_chain_support_details(SwapChainSupportDetails details, ThreadAllocTracker* alloc_tracker);

QueueFamilyIndices get_physical_device_queue_family_indices(VulkanEngine* engine, VkPhysicalDevice physical_device);
int score_physical_device_and_check_required_features(VulkanEngine* engine, VkPhysicalDevice physical_device);

void create_or_recreate_swapchain(VulkanEngine* engine);
void destroy_swapchain(VulkanEngine* engine, VkSwapchainKHR swapchain);

VkDescriptorPool create_descriptor_pool(VulkanEngine* engine, u32 max_descriptors, u32 max_sets);
VkDescriptorSet alloc_descriptor_set(VulkanEngine* engine, VkDescriptorPool pool, VkDescriptorSetLayout layout);

void create_all_descriptor_set_layouts(VulkanEngine* engine);
void destroy_all_descriptor_set_layouts(VulkanEngine* engine);

Renderable make_renderable_with_descriptors(VulkanEngine* engine, b32 is_opaque, GPU_MeshBuffers* mesh_ref, glm::mat4 initial_transform, ObjectDescriptorSetCreateInfo object_set_create_info);

GraphicsPipeline create_graphics_pipeline(VulkanEngine* engine, GraphicsPipelineConfigInfo config);
void destroy_graphics_pipeline(VulkanEngine* engine, GraphicsPipeline* gp);

void create_all_graphics_pipelines(VulkanEngine* engine);
void destroy_all_graphics_pipelines(VulkanEngine* engine);

GPU_Buffer create_buffer(VmaAllocator vma_allocator, u64 size, VkBufferUsageFlags buffer_usage_flags, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage);
void destroy_buffer(VmaAllocator vma_allocator, const GPU_Buffer* gpu_buffer);

VkCommandBuffer begin_one_time_command(VulkanEngine* engine);
void end_one_time_command_and_wait(VulkanEngine* engine, VkCommandBuffer command);

GPU_Buffer create_staging_buffer_from_data(VmaAllocator vma_allocator, u8* data, u64 size);
GPU_MeshBuffers create_mesh_buffers(VulkanEngine* engine,
    const u32*   indices,   u64 indices_size,    // sizes are in bytes
    const float* positions, u64 positions_size,
    const float* normals,   u64 normals_size,
    const float* texcoords, u64 texcoords_size,
    const float* tangents,  u64 tangents_size
);
void destroy_mesh_buffers(VulkanEngine* engine, GPU_MeshBuffers* mesh_buffers);

GPU_Image create_attachment_image(VulkanEngine* engine, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect_flags, b32 has_msaa);
GPU_Image create_render_target_attachment(VulkanEngine* engine, b32 is_depth_attachment, VkFormat desired_format, VkExtent3D extent, VkImageUsageFlags usage);

GPU_Image create_image_texture2d(VulkanEngine* engine, u8* data, u64 data_size, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage);
void destroy_image(VulkanEngine* engine, GPU_Image gpu_image);
GPU_Image load_image_texture2d(VulkanEngine* engine, const char* filepath);
void transition_gpu_images(VkCommandBuffer cmd, u32 image_barrier_count, const VkImageMemoryBarrier2* image_barriers, GPU_Image** gpu_image_refs);

glm::mat4 get_camera_view_matrix(glm::vec3 cam_pos, float pitch, float yaw);
glm::vec3 get_camera_direction_from_view(glm::mat4 view_matrix);
SceneUniform_GLSL_ScalarBlock calculate_shadow_uniforms(VulkanEngine* engine);
SceneUniform_GLSL_ScalarBlock calculate_scene_uniforms(VulkanEngine* engine, u32 width, u32 height);
void update_scene_uniforms(VulkanEngine* engine, VkCommandBuffer cmd, SceneUniform_GLSL_ScalarBlock scene_block);

void create_draw_lists(VulkanEngine* engine, DrawCommandLists* draw_lists);
void destroy_draw_lists(VulkanEngine* engine, DrawCommandLists* draw_lists);
void clear_draw_lists(DrawCommandLists* draw_lists);
void draw_to_graphics_pipeline(VulkanEngine* engine, RenderpassIndex renderpass_id, DrawCommandInfo draw_command_info);

void record_renderpass(VulkanEngine* engine, VkCommandBuffer cmd, FrameData* current_frame, bool bind_lights,
    RenderpassIndex renderpass_id, const GraphicsPipeline* const gp, RenderingAttachments rendering_attachments
);

// scene.cpp
void destroy_scene(VulkanEngine* engine);
GPU_Image* scene_add_texture(VulkanEngine* engine, GPU_Image gpu_image);
GPU_MeshBuffers* scene_add_mesh(VulkanEngine* engine, GPU_MeshBuffers gpu_mesh);
Renderable* scene_add_renderable(VulkanEngine* engine, b32 is_static, Renderable renderable);
PointLight* scene_add_pointlight(VulkanEngine* engine, PointLight p);
void create_scene(VulkanEngine* engine);

// lights.cpp
PointLight make_point_light(glm::vec3 pos, glm::vec4 color_and_intensity);
SpotLight make_spot_light(glm::vec3 pos, glm::vec4 color_and_intensity, glm::vec4 direction_and_cone_cutoff);
void copy_lights_to_gpu(VulkanEngine* engine, FrameData* current_frame);

#endif  // L_ENGINE_H
