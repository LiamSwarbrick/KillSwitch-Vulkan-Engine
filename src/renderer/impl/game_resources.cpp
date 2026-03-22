#include "game_resources.h"

#include "internal_state.h"

void create_startup_resources();
void create_window_dependent_resources();

void CreateOrRecreateResources(FG_ResourceFlags types_to_create)
{
    // Recreation step: Deallocate all preexisting resources of the requested type
    if (renderstate.rids.startup_resources_created)
    {
        // Loop through and check for resources of the types to recreate
        // DeallocateResource sets the dirty flag in the registry.
        // This means FG Import and FG Create Resource will fill in the gaps.
        for (uint32_t rid = 0; rid < renderstate.registry.resource_count; ++rid)
        {
            FG_Resource* res = &renderstate.registry.resources[rid];
            if ((res->flags & types_to_create) == types_to_create)
            {
                printf("Deallocating resource: %s\n", res->debug_name);
                FG_DeallocateResource(res);
            }
        }
    }


    FG_ResourceFlags flags;

    flags = FG_RESOURCE_FLAGS_WINDOW_DEPENDENT;
    if ((flags & types_to_create) == types_to_create)
    {
        create_window_dependent_resources();
        renderstate.rids.window_resources_created = 1;
    }


    flags = FG_RESOURCE_FLAGS_ON_STARTUP;
    if ((flags & types_to_create) == types_to_create)
    {
        SDL_assert(!renderstate.rids.startup_resources_created && "Startup resources should not be recreated at any point.");
        SDL_assert(renderstate.rids.window_resources_created && "Window resources must be created firstly.");
        create_startup_resources();
        renderstate.rids.startup_resources_created = 1;
    }


    
#ifndef NDEBUG
    // Check we haven't left any gaps in the array
    for (uint32_t rid = 0; rid < renderstate.registry.resource_count; ++rid)
    {
        SDL_assert(renderstate.registry.resources[rid].type != FG_RESOURCE_TYPE_INVALID &&
            "Not all resources with flag FG_RESOURCE_TYPE_WINDOW_DEPENDENT were created in create_or_recreate_window_dependent_resources(), which is a requirement (inferred from there being gaps in the registry)."
        );
    }
#endif
}

void DestroyResources()
{
    SDL_assert(renderstate.rids.startup_resources_created);

    FG_ClearResources();

    // Finally
    renderstate.rids.startup_resources_created = 0;
}


void create_startup_resources()
{
    FG_ResourceFlags flags = FG_RESOURCE_FLAGS_ON_STARTUP;

    // Scene Buffer
    ResourceCreateInfo scene_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(SceneData),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
    };
    renderstate.rids.global_scene_buffer_rid = FG_CreateResource(
        "GlobalSceneBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &scene_info
    );

    // Objects Buffer (Mapped so we rapidly upload transforms each frame)
    const uint32_t MAX_RENDERED_OBJECTS = 100000;
    ResourceCreateInfo objects_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_RENDERED_OBJECTS * sizeof(ObjectData),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_cpu_accessible = 1  // <- Hence mapped
    };
    renderstate.rids.objects_buffer_rid = FG_CreateResource(
        "ObjectsBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &objects_info
    );
    renderstate.object_transforms = MakeArenaOnBufferResource(renderstate.rids.objects_buffer_rid);

    // Materials SSBO
    const uint32_t MAX_MATERIALS = 1024;
    ResourceCreateInfo mat_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(MaterialData) * MAX_MATERIALS,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        }
    };
    renderstate.rids.material_ssbo_rid = FG_CreateResource(
        "MaterialSSBO", FG_RESOURCE_TYPE_BUFFER, flags, &mat_info
    );



    /////////////////////
    #warning BELOW IS DUMMY DATA, THAT SHOULD NOT BE PART OF startup_resources()

    // TODO IMPORTANT: ONLY SLOT 0 OF THE MATERIAL SSBO WILL BE WRITTEN TO WITH THIS LOGIC
    // TODO: Change to PBR material
    // Initial "Dummy" Material Data
    // Let's set Material 0 to be a simple White material with no texture
    MaterialData default_mat = {
        .base_color = { 1.0f, 1.0f, 1.0f, 1.0f },
        .texture_idx = 0xFFFFFFFF,  // Our "No Texture" sentinel
        .sampler_idx = FG_SAMPLER_LINEAR_REPEAT,
        .alpha_cutoff = 0.5f
    };
    FG_UploadBufferData(&renderstate.main.staging_objects, 
        renderstate.rids.material_ssbo_rid, &default_mat, sizeof(MaterialData)
    );

    // TEST QUAD (TODO: Change to create_mesh_resource or something):
    #warning This is not indexed data. Switch to indexed meshes
    Vertex quad_verts[4] = {
        {{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0,0,1}, {1,0,0,1}, {0,0,0,0}, {0,0,0,0}}, 
        {{ 1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0,0,1}, {0,1,0,1}, {0,0,0,0}, {0,0,0,0}}, 
        {{ 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0,0,1}, {0,0,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{ 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0,0,1}, {0,1,1,1}, {0,0,0,0}, {0,0,0,0}},
    };
    uint32_t quad_indices[6] = { 0, 1, 2, 0, 3, 1 };

    ResourceCreateInfo quad_verts_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(quad_verts),
            // Use DEVICE_LOCAL for speed, plus the flags needed for BDA and pulling
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT 
        }
    };
    renderstate.rids.quad_verts_rid = FG_CreateResource(
        "TriangleVertexBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &quad_verts_info
    );
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.quad_verts_rid, 
                        quad_verts, 
                        sizeof(quad_verts)
    );
    ResourceCreateInfo quad_index_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(quad_indices),
            // Use DEVICE_LOCAL for speed, plus the flags needed for BDA and pulling
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT 
        }
    };
    renderstate.rids.quad_index_rid = FG_CreateResource(
        "TriangleIndexBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &quad_index_info
    );
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.quad_index_rid, 
                        quad_indices, 
                        sizeof(quad_indices)
    );


    // TEST EMPTY IMAGE RESOURCE:
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
    uint32_t test_texture_rid = FG_CreateResource("Test texture", FG_RESOURCE_TYPE_IMAGE, flags, &test_create_info);

}


void create_window_dependent_resources()
{
    FG_ResourceFlags flags = FG_RESOURCE_FLAGS_WINDOW_DEPENDENT;

    // Import swapchain images as framegraph resources
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
        renderstate.rids.swapchain_image_rids[i] = FG_ImportResource(
            swapchain_image_name, FG_RESOURCE_TYPE_IMAGE, flags, import_info
        );
    }
}


