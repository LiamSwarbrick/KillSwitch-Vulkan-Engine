#ifndef RENDERER_FRAMEGRAPH_H
#define RENDERER_FRAMEGRAPH_H

// NOTE(Liam): Frame Graph Motivation:
// I've wasted too much time implemented features in pure hardcoded vulkan.
// The idea here is to define your frame as a graph, and compile the resource
// dependencies. It's becoming more common recently, but there lot's of questions
// that come about when it comes to actually programming this shit in Vulkan.
// Questions about the renderer, material system, mesh system, different types of shaders.
// Oh well, will hopefully figure this out.

#include "../renderer.h"
#include "vulkan_wrapper.h"

// Arbitrary predefined array sizes for simplicity
#define MAX_PASS_RESOURCE_BANDWIDTH  16
#define MAX_PASSES          64
#define MAX_RESOURCES       1024

typedef enum
{
    FG_USAGE_COLOR   = 1 << 0,  // COLOR/DEPTH/STENCIL for output attachments
    FG_USAGE_DEPTH   = 1 << 1,
    FG_USAGE_STENCIL = 1 << 2,
    FG_USAGE_STORAGE = 1 << 3,  // For Compute SSBOs or Storage Images
    FG_USAGE_SAMPLED = 1 << 4   // For Shaders reading textures
}
FG_UsageFlags;

typedef struct PassResourceUsage
{
    uint32_t id;               // Index into a resource array (the internal registry)
    FG_UsageFlags usage_flags; // Tells the graph HOW to use this resource in this pass

    // Sync state
    VkAccessFlags2 access;        // e.g., VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    VkPipelineStageFlags2 stage;  // e.g., VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
    VkImageLayout layout;         // For images only (buffers can leave these 0)
    // NOTE: Not implementing queue ownership transfers of resources.

    // Per-output control (only used if usage_flags includes COLOR, DEPTH, or STENCIL)
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
}
PassResourceUsage;

typedef struct RenderPassDesc
{
    const char* debug_name;

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


// FrameGraph Resource Tracking Stuff
//

typedef enum
{
    FG_RESOURCE_TYPE_IMAGE,
    FG_RESOURCE_TYPE_BUFFER
}
FG_ResourceType;

typedef struct FG_Resource
{
    FG_ResourceType type;
    union
    {
        struct {
            VkImage handle;
            VkImageView view;
            VkFormat format;
            VkImageLayout last_layout;
            VkImageAspectFlags aspect_mask;
        } image;

        struct {
            VkBuffer handle;
            uint32_t size;
            void* mapped_data;  // Useful for uniform/storage updates
        } buffer;
    };

    // Shared Sync State
    VmaAllocation allocation;
    VkAccessFlags2 last_access;
    VkPipelineStageFlags2 last_stage;
    const char* name;  // For debugging
}
FG_Resource;

typedef struct ResourceRegistry
{
    uint32_t resource_count;
    FG_Resource resources[MAX_RESOURCES];
}
ResourceRegistry;


#endif  // RENDERER_FRAMEGRAPH_H
