#ifndef L_ENGINE_STRUCTS_H
#define L_ENGINE_STRUCTS_H

#include "my_c_runtime.h"
#include "vk_layer.h"
#include "shaders_info.h"

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
        s32 graphics_family;
        s32 present_family;
    };
    s32 array[NUM_QUEUE_FAMILY_INDICES];
}
QueueFamilyIndices;


typedef struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    u32 present_mode_count;
    VkSurfaceFormatKHR* formats;
    VkPresentModeKHR* present_modes;
}
SwapChainSupportDetails;


typedef struct GPU_Image
{
    VkImageLayout   current_layout;
    VkImage         image;
    VkImageView     image_view;
    VmaAllocation   allocation;
    VkExtent3D      image_extent;
    VkFormat        image_format;
}
GPU_Image;


typedef struct GPU_Buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
}
GPU_Buffer;


typedef struct GPU_MeshBuffers
{
    u32 vertex_count;
    u32 index_count;

    GPU_Buffer index_buffer;

    union
    {
        struct
        {
            GPU_Buffer position_buffer;
            GPU_Buffer normal_buffer;
            GPU_Buffer texcoord_buffer;
            GPU_Buffer tangent_buffer;
        };
        GPU_Buffer attribute_buffers[MAX_VERTEX_ATTRIBUTES];
        // A mesh has all the vertex attributes, if this becomes annoying, refactor the engine a bit.
    };
}
GPU_MeshBuffers;

typedef enum BlendModeEnum
{
    BLEND_MODE_OPAQUE,
    BLEND_MODE_ALPHA_MASK,
    BLEND_MODE_ALPHA_BLEND,
    BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH,  // For the opaque overshading shader, an unusual case for the overshading visualisation where we actually want transparent fragments to write and discard future fragments behind them.
    BLEND_MODE_ADD_TO_SRC,  // Used to add the bloom texture on top the render target without pingpong buffers (don't want to have to sample the rendertarget as well, and then copy)
}
BlendMode;

typedef struct SPIRVConfig
{
    const char* spirv_path;
    const char* entrypoint_name;
    const VkSpecializationInfo* pSpecializationInfo;
}
SPIRVConfig;

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


// The renderable index type is used to index DrawLists
typedef enum RenderpassIndexEnum
{
    RENDERPASS_INDEX_RENDERPASS_OPAQUE,
    RENDERPASS_INDEX_RENDERPASS_TRANSPARENT,
    RENDERPASS_INDEX_POSTPROCESS,

    RENDERPASS_INDEX_COUNT
}
RenderpassIndex;

// When we create a renderable, we provide this struct to
// create the object's descriptor set
typedef struct ObjectDescriptorSetCreateInfo
{
    GPU_Image* map_refs[OBJECT_TEXTURE_MAP_COUNT];  // index 0=albedo_alpha, etc.
    VkSampler samplers[OBJECT_TEXTURE_MAP_COUNT];

    // ...
}
ObjectDescriptorSetCreateInfo;


// NOTE:
// A renderable references vertex data in mesh_ref, and has a descriptor_set for materials etc.
// A renderable is drawn into the scene at a location using a draw command. (so we can render it multiple times with different transforms)
typedef struct Renderable
{
    // GraphicsPipelineTypeFlags  valid_pipelines;        // Which pipeline(s) the object is defined for (using bitflags)
    b32                       is_opaque;               // Which pipeline should it draw to.
    GPU_MeshBuffers*           mesh_ref;               // The geometry data
    glm::mat4                  initial_transform;      // Initial transform is applied to the vertex positions prior to the model transform in a draw command.
    VkDescriptorSet            object_descriptor_set;  // The per object descriptor set (contains textures, materials, etc.)
}
Renderable;


// A draw command wraps a renderable object (static) with dynamically changing information
// i.e. its model transform, animation data (once implemented)
typedef struct DrawCommandInfo
{
    Renderable*  renderable_ref;
    glm::mat4    model_matrix;
}
DrawCommandInfo;


// NOTE: For each individual graphics pipeline's list of draw commands,
// we could in future used instanced rendering by grouping draw commands by their renderable_ref->mesh_ref
typedef struct DrawCommandLists
{
    u32              capacities[RENDERPASS_INDEX_COUNT];  // max num elements
    u32              sizes[RENDERPASS_INDEX_COUNT];       // current num elements
    DrawCommandInfo* lists[RENDERPASS_INDEX_COUNT];       // per pipeline list of draw commands
}
DrawCommandLists;
#define DRAW_COMMAND_LISTS_STARTING_CAPACITY 1024


typedef struct RenderingAttachments
{
    u32                        color_attachment_count;
    VkRenderingAttachmentInfo* color_attachments;  // Pointer to array of color attachments
    VkImage*             color_attachment_images;  // TODO: Currently the images are unused, but it may be useful for an automatic way of transitioning images during the render pass (maybe that would be useful, maybe not)

    VkRenderingAttachmentInfo* depth_attachment;   // Pointer to a single depth attachment
    VkImage*             depth_attachment_image;

    VkRect2D                   render_area;
    VkViewport                 viewport;
    VkRect2D                   scissor;
    // NOTE: Currently the render_area, viewport, and scissor simply cover the screen and don't change
    // But this is future proofing for shadow map implementation
    // since a shadow map or otherwise won't be the size of the render target.
}
RenderingAttachments;

//
// Scene
//

typedef struct Scene
{
    b32 is_initialised;

    // Size and capacity below are num elements (not num bytes).

    u32 textures_size, textures_capacity;
    GPU_Image* textures;

    u32 meshes_size, meshes_capacity;
    GPU_MeshBuffers* meshes;

    u32 renderables_size, renderables_capacity;
    Renderable* renderables;

    // A scene with static geometry will render a renderable in it's default transform.
    u32 statics_size, statics_capacity;
    u32* static_renderable_ids;

    u32 point_lights_size, point_lights_capacity;
    PointLight* point_lights;
}
Scene;

#endif  // L_ENGINE_STRUCTS_H
