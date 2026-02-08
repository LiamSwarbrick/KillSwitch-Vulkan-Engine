#ifndef RENDERER_INTERNALS_DUE_REWORK_H
#define RENDERER_INTERNALS_DUE_REWORK_H

#include "../vulkan_wrapper.h"
#include "core/my_c_runtime.h"
#include "internal_structs.h"
#include "shared_glsl_defs.h"

typedef enum BlendModeEnum
{
    BLEND_MODE_OPAQUE,
    BLEND_MODE_ALPHA_MASK,
    BLEND_MODE_ALPHA_BLEND,
    BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH,  // For the opaque overshading shader, an unusual case for the overshading visualisation where we actually want transparent fragments to write and discard future fragments behind them.
    BLEND_MODE_ADD_TO_SRC,  // Used to add the bloom texture on top the render target without pingpong buffers (don't want to have to sample the rendertarget as well, and then copy)
}
BlendMode;

typedef struct GraphicsPipelineConfigInfo
{
    // NOTE: In future with extra types of shaders, use NULL spirv_path to not use that shader
    SPIRVConfig vertex_spirv_config;
    SPIRVConfig fragment_spirv_config;
    union
    {
        struct
        {
            SPIRVConfig geometry_spirv_config;
        };
        SPIRVConfig other_shaders_spirv_configs[1];
    };

    union
    {
        struct
        {
            b32 has_attribute_position;
            b32 has_attribute_normal;
            b32 has_attribute_texcoord;
            b32 has_attribute_tangent;
        };
        b32 has_attrib[MAX_VERTEX_ATTRIBUTES];  // e.g. has_attrib[ATTRIB_LOC_POSITION]
    };
    VkPipelineLayoutCreateInfo     pipeline_layout_create_info;
    VkPipelineRenderingCreateInfo  pipeline_rendering_create_info;
    u32                            blend_mode_count;
    BlendModeEnum*                 blend_modes;
    VkCullModeFlags                cull_mode;
}
GraphicsPipelineConfigInfo;

typedef struct GraphicsPipeline
{
    b32 is_created;
    VkPipelineLayout layout;
    VkPipeline pipeline;

    b32 has_attrib[MAX_VERTEX_ATTRIBUTES];  // e.g. has_attrib[ATTRIB_LOC_POSITION]
}
GraphicsPipeline;

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


typedef struct OldFrameData
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
OldFrameData;

typedef struct OldRenderState
{
    // Double buffer our command buffers so that while one is being executed
    // by the GPU, we still have access to another set to write the next commands to.
    #define FRAME_OVERLAP 2
    u32 current_frame_id;  // Index of frames we write to this frame
    OldFrameData frames[FRAME_OVERLAP];

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

    // Current rendering mode: e.g. select PBR or one of the various debug rendering modes
    RenderMode render_mode;
    RenderMode last_render_mode;

    // Graphics Pipelines
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

}
OldRenderState;

// Stuff I don't want in my vulkan wrapper anymore:
VkCommandBuffer vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool);
VkCommandBufferSubmitInfo vklayer_command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 vklayer_submit_info(VkCommandBufferSubmitInfo* cmd_submit_info, VkSemaphoreSubmitInfo* signal_semaphore_submit_info, VkSemaphoreSubmitInfo* wait_semaphore_submit_info);

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
VkFormat vklayer_find_supported_depth_format(VkPhysicalDevice physical_device);

#endif  // RENDERER_INTERNALS_DUE_REWORK_H
