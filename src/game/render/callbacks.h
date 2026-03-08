#ifndef GAME_RENDER_CALLBACKS_H
#define GAME_RENDER_CALLBACKS_H

#include "renderer/framegraph.h"
#include "renderer/renderer.h"

static void Callback_CreateResources(FG_ResourceFlags required_flags)
{
    FG_ResourceFlags flags;
    VkExtent2D swapchain_extent = Renderer_GetSwapchainExtent();
    
    flags = FG_RESOURCE_FLAGS_WINDOW_DEPENDENT;
    if ((flags & required_flags) == required_flags)
    {
        
    }

    flags = FG_RESOURCE_FLAGS_NONE;
    if ((flags & required_flags) == required_flags)
    {
        // TEST:
        {
            ResourceCreateInfo test_create_info = {
                .image_create_info = {
                    .sType        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,

                    .extent = {
                        .width  = swapchain_extent.width,
                        .height = swapchain_extent.height,
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
            uint32_t test_texture_rid = FG_CreateResource("Test texture", FG_RESOURCE_TYPE_IMAGE, FG_RESOURCE_FLAGS_NONE, &test_create_info);
        }
    }

}

#endif  // GAME_RENDER_CALLBACKS_H
