#include "framegraph.h"

#define MAX_RESOURCE 1024

typedef enum
{
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_BUFFER
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

void FrameGraph_Execute(FrameGraph* fg, VkCommandBuffer cmd)
{
    for (uint32_t i = 0; i < fg->pass_count; ++i)
    {
        RenderPassDesc* pass = &fg->passes[i];

        VkImageMemoryBarrier2 barriers[32];
        SDL_assert(pass->output_count < sizeof(pass->outputs)/sizeof(pass->outputs[0]));
        for (uint32_t r = 0; r < pass->output_count; ++r)
        {
            PassResourceUsage* usage = &pass->outputs[r];
            // VkImage image = get_vulkan_image()
        }
    }
}
