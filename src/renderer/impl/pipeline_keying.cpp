#include "pipeline_keying.h"
#include "stb_ds.h"
#include "SDL3/SDL.h"
#include <stdio.h>

void PK_Init(PipelineEntry** pipeline_map_ref)
{
    *pipeline_map_ref = NULL;  // Set to NULL so stb_ds will make a new hash map.
}

VkPipeline PK_GetOrCreatePipeline(PipelineEntry** pipeline_map_ref, PipelineKey key)
{
    ptrdiff_t index = hmgeti(*pipeline_map_ref, key.value);

    if (index >= 0)
    {
        // Found, return premade pipeline
        VkPipeline cached_pipeline = (*pipeline_map_ref)[index].value;
        return cached_pipeline;
    }

    // Not found in hashmap, create new pipeline
    VkPipeline new_pipeline = VK_NULL_HANDLE;
    switch ((PK_PipelineType)key.pipeline_type)
    {
    case PK_PIPELINE_TYPE_COMPUTE:
        // TODO.
        break;

    case PK_PIPELINE_TYPE_GRAPHICS:
        // TODO.
        break;
        
    default:
        SDL_assert(0 && "Invalid pipeline type");
    }

    // Add new pipeline to hash map
    hmput(*pipeline_map_ref, key.value, new_pipeline);

    return new_pipeline;
}

void PK_Shutdown(PipelineEntry** pipeline_map_ref, VkDevice device)
{
    for (uint32_t i = 0; i < hmlenu(*pipeline_map_ref); ++i)
    {
        vkDestroyPipeline(device, (*pipeline_map_ref)[i].value, NULL);
    }

    hmfree(*pipeline_map_ref);  // Frees and sets the pointer to NULL.
}
