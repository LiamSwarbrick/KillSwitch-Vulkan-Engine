// NOTE(Liam): Frame Graph Motivation:
// Because in the past I've wasted too much time implemented features in pure hardcoded vulkan
// (things like bloom involves many passes and buffers and descriptor sets and layouts and pipeline and shaders and synchronisation).
// The idea here is to define your frame as a graph, and compile the resource
// dependencies. It's becoming more common recently, but there lot's of questions ..
// ..that come about when it comes to actually programming this shit in Vulkan.
// Questions about the renderer, material system, mesh system, different types of shaders.
// Oh well, will hopefully figure this out.


// How the code works:
//
// To understand whats going on in the code, start with the struct FrameGraph (a bunch of renderpass descriptions).
// For the internals, the RenderState contains:
// - ResourceRegistry registry; Which holds all resources and their current state.
// - BindlessHeap heap; Which is the descriptor set that holds all textures and samplers.
// (buffers on the other hand, are passed to the shader through their GPU pointer with the BufferDeviceAddress vulkan feature)


// Usage notes:
//
// - Create resource at startup or scene change (if resources differ there)
//
// - Specify render passes in order (inside the FrameGraph::passes array).
//   Initial implementation will just execute them in the order they are specified.
//   But with no change to the API, I could implement a topological sort under the hood ..
//   ..by analysing the inputs and outputs of each pass.
//
#ifndef RENDERER_FRAMEGRAPH_H
#define RENDERER_FRAMEGRAPH_H

#include "../renderer.h"
#include "vulkan_wrapper.h"

// TODO: Still need topological sort and execution of all renderpasses.
// Lest I just implement one pass after another, in the order it appears.
// Actually, that's probably nicer.

// Called at the start and end.
void FG_Init();
void FG_Shutdown();

void FG_ClearResources();

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
    FG_SAMPLER_COUNT
}
FG_SamplerType;

typedef struct PassResourceUsage
{
    uint32_t id;               // Index into a resource array (the internal registry)
    FG_UsageFlags usage_flags; // Tells the graph HOW to use this resource in this pass
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

typedef struct FrameGraph
{
    uint32_t pass_count;
    RenderPassDesc passes[MAX_PASSES];
}
FrameGraph;

void FG_ApplyBarriers(VkCommandBuffer cmd, RenderPassDesc* pass);
void FG_ExecutePass(FrameGraph* fg, uint32_t pass_idx, VkCommandBuffer cmd);

// FrameGraph Resource Tracking Stuff
//

typedef enum
{
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

typedef struct FG_Resource
{
    char debug_name[64];  // TODO: Add to renderdoc with vkDebugMarkerSetObjectNameEXT somehow
    FG_ResourceType type;
    union
    {
        ResourceBufferData buffer;
        ResourceImageData image;
    }; 
    VmaAllocation allocation;  // NOTE: For imported resources like swapchain images set to VK_NULL_HANDLE.

    // Shader side access to resources
    uint32_t image_bindless_index;       // Index into the global texture array, UINT32_MAX for nonsamplable images e.g. the swapchain
    VkDeviceAddress buffer_gpu_address;  // Buffer device address for shader

    // Sync State
    VkAccessFlags2         current_access;
    VkPipelineStageFlags2  current_stage;
    VkImageLayout          current_layout;  // Images only, buffers can leave this 0
}
FG_Resource;

typedef struct ResourceRegistry
{
    uint32_t resource_count;
    FG_Resource resources[MAX_RESOURCES];
}
ResourceRegistry;

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
uint32_t FG_CreateResource(const char* debug_name, FG_ResourceType type, ResourceCreateInfo* create_info);

typedef union ResourceImportInfo
{
    ResourceBufferData buffer;
    ResourceImageData image;
}
ResourceImportInfo;
uint32_t FG_ImportResource(const char* debug_name, FG_ResourceType type, ResourceImportInfo import_info);


// Descriptors
//

typedef struct BindlessHeap
{
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout set_layout;
    VkDescriptorSet global_set;

    uint32_t texture_count;
    VkSampler samplers[FG_SAMPLER_COUNT];
}
BindlessHeap;

void FG_BindlessHeap_Init();
void FG_BindlessHeap_Shutdown();
void FG_BindlessHeap_CreateAllSamplers();

// Pipelines
//

void FG_CreateGlobalPipelineLayout();

#endif  // RENDERER_FRAMEGRAPH_H
