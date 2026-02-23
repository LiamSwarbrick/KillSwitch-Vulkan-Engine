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

typedef struct PassResourceUsage
{
    uint32_t id;            // Index into a global resource array
    VkAccessFlags2 access;  // e.g., VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    VkImageLayout layout;   // e.g., VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    VkPipelineStageFlags2 stage;
}
PassResourceUsage;

typedef struct RenderPassDesc
{
    const char* debug_name;
    uint32_t input_count;
    PassResourceUsage inputs[16];
    uint32_t output_count;
    PassResourceUsage outputs[16];

    // A function pointer to the draw calls
    void (*execute_callback)(VkCommandBuffer cmd, void* user_data);
    void* user_data;
}
RenderPassDesc;

typedef struct FrameGraph
{
    uint32_t pass_count;
    RenderPassDesc passes[64];
}
FrameGraph;

void FrameGraph_Execute(FrameGraph* fg, VkCommandBuffer cmd);

#endif  // RENDERER_FRAMEGRAPH_H
