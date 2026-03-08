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
//   Then call FG_CmdRenderFrame, passing an active graphics command buffer as the arg.
//   Initial implementation will just execute them in the order they are specified.
//   But with no change to the API, I could implement a topological sort under the hood ..
//   ..by analysing the inputs and outputs of each pass.
//


// TODO: Make framegraph.h/.cpp completely self contained:
// 1. I.e. pass VkDevice, FrameGraph*, ResourceRegistry, BindlessHeap, and global pipeline layout explicitly
//    .. resulting in a completely functional interface.
// 2. Move the logic for only recreating window resources on resize into framegraph.h
//    because it's useful enough for reuse.
//    Or just make the resource creation side of things more automatic in this library.
//    Like giving resources an init callback,
//    and then resources with FG_RESOURCE_FLAGS_WINDOW_DEPENDENT can be handled
//    in a FG_ResizeResources() called from the window resize callback
//    where that function only recreates the window dependent resources
//    thus containing that logic within this lib.
//  3. Only put the API that should be externally called like FG_Init etc.
//     as exposed functions. Things like FG_ApplyBarriers should instead go
//     inside framegraph.cpp, with the prototype apply_barriers() defined
//     at the top of the cpp file instead of the exposed header API.
//
// --- TODOs that are intentionally out of scope currently ---
// No multiqueue support, so no concurrent passes or topological ordering currently.
// This would improve GPU occupancy and allows higher end GPUs to be pushed much further.
// No automatic resource optimization, reuse, etc... (reuse can be done manually)
// (Framegraph is built each frame, all resources are manully made on init/resize)
// NOTE: For render-target resources only.. 
//   ..it would be possible to optimize resource usage by lazily allocating
//   a pool of buffers, and running each frame graph combination on init,
//   so that the max required amount of resources are made.
//   Then the passes just request the size of resource they need.
//   And a free one is assigned from the pool.
//   Can even reduce transitions by first trying to match a free rendertarget
//   that is already in the correct image layout.

#ifndef RENDERER_FRAMEGRAPH_INTERNALS_H
#define RENDERER_FRAMEGRAPH_INTERNALS_H

#include "framegraph.h"
#include "internal_structs.h"
#include "core/my_c_runtime.h"
#include "vulkan_wrapper.h"

// Called at the start and end.
void _FG_Init();
void _FG_Shutdown();

void _FG_ClearResources();

typedef struct FrameGraph
{
    uint32_t pass_count;
    RenderPassDesc passes[MAX_PASSES];

    b32 resources_created;
    void (*resources_create_callback)(FG_ResourceFlags res_types_to_create_or_recreate);
}
FrameGraph;

// Graph Execution
void _FG_CmdRenderFrame(VkCommandBuffer cmd);
void _FG_CmdTransitionSwapchainForPresentation(VkCommandBuffer cmd, uint32_t swapchain_image_rid);


typedef struct FG_Resource
{
    char debug_name[64];  // TODO: Add to renderdoc with vkDebugMarkerSetObjectNameEXT somehow
    FG_ResourceType type;
    FG_ResourceFlags flags;
    union
    {
        ResourceBufferData buffer;
        ResourceImageData image;
    }; 
    VmaAllocation allocation;  // NOTE: For imported resources like swapchain images set to VK_NULL_HANDLE.

    // Shader side access to resources
    union
    {
        uint32_t      image_bindless_index;  // Index into the global texture array, UINT32_MAX for nonsamplable images e.g. the swapchain
        VkDeviceAddress buffer_gpu_address;  // Buffer device address for shader
    };

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

    b32 dirty_because_gaps;
}
ResourceRegistry;

void _FG_DeallocateResource(FG_Resource* res);

// Staging
//

void _FG_UploadBufferData(ThreadStagingObjects* stg, uint32_t rid, const void* data, uint32_t size);
void _FG_UploadImageData(ThreadStagingObjects* stg, uint32_t rid, const void* data, uint32_t size);

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

#endif  // RENDERER_FRAMEGRAPH_INTERNALS_H
