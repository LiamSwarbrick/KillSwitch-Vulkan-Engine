#include "pass_definitions.h"

#include "internal_state.h"

// Create an initial pipeline with global_pipeline_layout
// Implement load spv resources with SDL

void CreatePassDefinitionsAndResources()
{
    SDL_assert(!renderstate.pass_defs.is_created);

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

    ResourceCreateInfo test_create_info = {
        .image_create_info = {
            .sType        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,

            .extent = {
                .width  = renderstate.swapchain_extent.width,
                .height = renderstate.swapchain_extent.height,
                .depth  = 1
            },

            .mipLevels = 1,
            .arrayLayers = 1,

            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,

            .usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,

            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,

            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        },
        .image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,

            .image = VK_NULL_HANDLE,  // <- This gets set in the registry code
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,

            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },

            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };
    uint32_t test_texture_rid = FG_CreateResource("Test texture", FG_RESOURCE_TYPE_IMAGE, &test_create_info);

    // Finally
    renderstate.pass_defs.is_created = 1;
}

void DestroyPassDefinitionsAndResources()
{
    SDL_assert(renderstate.pass_defs.is_created);

    FG_ClearResources();

    // Finally
    renderstate.pass_defs.is_created = 0;
}