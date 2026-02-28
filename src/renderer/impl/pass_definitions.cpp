#include "pass_definitions.h"

#include "internal_state.h"

// Create an initial pipeline with global_pipeline_layout
// Implement load spv resources with SDL

void CreatePassDefinitionsAndResources()
{
    SDL_assert(!renderstate.pass_defs.is_created);
    renderstate.pass_defs.is_created = 1;

    // Import swapchain resource
    char swapchain_image_name[64] = {};
    for (uint32_t i = 0; i < renderstate.swapchain_image_count; ++i)
    {
        snprintf(swapchain_image_name, sizeof(swapchain_image_name), "SwapchainImage_%d", i);

        ResourceImportInfo import_info = {
            .image = {
                .handle  = renderstate.swapchain_images[i],
                .view    = renderstate.swapchain_image_views[i],
                .format  = renderstate.swapchain_image_format,
                .subresource_range = {
                    .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel    = 0,
                    .levelCount      = 1,
                    .baseArrayLayer  = 0,
                    .layerCount      = 1
                }
            }
        };
        renderstate.pass_defs.swapchain_image_rids[i] = FG_ImportResource(
            swapchain_image_name, FG_RESOURCE_TYPE_IMAGE, import_info
        );
    }
}

void DestroyPassDefinitionsAndResources()
{
    SDL_assert(renderstate.pass_defs.is_created);
    renderstate.pass_defs.is_created = 0;


}
