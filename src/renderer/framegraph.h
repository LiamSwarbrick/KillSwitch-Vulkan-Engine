#ifndef RENDERER_FRAMEGRAPH_H
#define RENDERER_FRAMEGRAPH_H

#include "core/my_c_runtime.h"
#include "vulkan_wrapper.h"

// Arbitrary predefined array sizes for simplicity
#define MAX_PASS_RESOURCE_BANDWIDTH  16
#define MAX_PASSES          64
#define MAX_RESOURCES       1024
#define NUM_BINDLESS_TEXTURE_SLOTS 100000   // Ample descriptor slots to never worry about again.

typedef enum
{
    FG_USAGE_COLOR   = 1 << 0,  // COLOR/DEPTH/STENCIL for output attachments
    FG_USAGE_DEPTH   = 1 << 1,
    FG_USAGE_STENCIL = 1 << 2,
    FG_USAGE_STORAGE = 1 << 3,  // For Compute SSBOs or Storage Images
    FG_USAGE_SAMPLED = 1 << 4   // For Shaders reading textures
}
FG_UsageFlags;

// TODO: Add more address modes than just REPEAT
//       Also support for LUT textures (look up tables) e.g. for LTC area lights
typedef enum
{
    FG_SAMPLER_NEAREST_REPEAT,
    FG_SAMPLER_LINEAR_REPEAT,
    FG_SAMPLER_ANISOTROPIC_REPEAT,
    FG_SAMPLER_SHADOW,

    FG_SAMPLER_COUNT,
    FG_SAMPLER_NOT_SAMPLABLE,  // For passes to swapchain (possibly something else).
}
FG_SamplerType;

typedef struct PassResourceUsage
{
    uint32_t id;                  // Index into a resource array (the internal registry)
    FG_UsageFlags usage_flags;    // Tells the graph HOW to use this resource in this pass
    FG_SamplerType sampler_type;  // Only for image resources. Not using combined image samplers so we can have different samplers for the same image in different passes

    // Sync state
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
    VkImageLayout layout;  // For images only (buffers can leave these 0)
    // NOTE: Not implementing queue ownership transfers of resources.

    // Per-output control (only used if usage_flags includes COLOR, DEPTH, or STENCIL)
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
}
PassResourceUsage;

typedef struct RenderPassDesc
{
    char debug_name[64];  // TODO: Add to renderdoc with vkDebugMarkerSetObjectNameEXT somehow

    // Resource inputs/outputs (buffers and image attachments)
    uint32_t          input_count;
    PassResourceUsage inputs[MAX_PASS_RESOURCE_BANDWIDTH];
    uint32_t          output_count;
    PassResourceUsage outputs[MAX_PASS_RESOURCE_BANDWIDTH];

    // Rendering intent (for dynamic rendering begin info)
    b32 is_compute;        // Compute can ignore other intent parameters (leave them 0).
    VkRect2D render_area;
    b32 use_custom_viewport_scissor;  // Can leave custom_viewport/scissor 0 if false to simply infer them from render_area
    VkViewport custom_viewport;
    VkRect2D   custom_scissor;

    // A function pointer to what executes the draw calls
    void (*execute_callback)(VkCommandBuffer cmd, void* user_data);
    void* user_data;
}
RenderPassDesc;

uint32_t FG_AddPass(RenderPassDesc pass_description);

// FrameGraph Resources
//

typedef enum
{
    FG_RESOURCE_TYPE_INVALID = 0,
    FG_RESOURCE_TYPE_IMAGE,
    FG_RESOURCE_TYPE_BUFFER
}
FG_ResourceType;

typedef struct ResourceBufferData
{
    VkBuffer handle;

    uint32_t size;
    // TODO: Support mapped data:
    void* mapped_data;  // Useful for uniform/storage updates, e.g. CPU side light assignment
}
ResourceBufferData;

typedef struct ResourceImageData
{
    VkImage                 handle;
    VkImageView             view;

    // Metadata about the image needed for parts of the frame graph
    VkFormat                format;
    VkExtent3D              extent;  // TODO: Use for checking if render_area matches (also can use custom scissor and viewport if it's oversized?)
    VkImageUsageFlags       usage;   // Tells us if we can go into BindlessHeap (when has SAMPLED_BIT)
    VkImageSubresourceRange subresource_range;  // Required for barriers
}
ResourceImageData;

typedef union ResourceCreateInfo
{
    struct
    {
        VkImageCreateInfo image_create_info;
        VkImageViewCreateInfo image_view_create_info;
    };

    VkBufferCreateInfo buffer_create_info;
}
ResourceCreateInfo;

typedef union ResourceImportInfo
{
    ResourceBufferData buffer;
    ResourceImageData image;
}
ResourceImportInfo;

typedef enum
{
    FG_RESOURCE_FLAGS_NONE = 0,
    FG_RESOURCE_FLAGS_WINDOW_DEPENDENT = 1 << 0,  // On resize, recreate these.
}
FG_ResourceFlags;

uint32_t FG_CreateResource(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, ResourceCreateInfo* create_info);
uint32_t FG_ImportResource(const char* debug_name, FG_ResourceType type, FG_ResourceFlags flags, ResourceImportInfo import_info);



#endif  // RENDERER_FRAMEGRAPH_H
