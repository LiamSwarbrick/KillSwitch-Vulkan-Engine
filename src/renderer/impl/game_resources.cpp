#include "game_resources.h"

#include "internal_state.h"

#include "core/assetsys.h"  // CPU-side asset data

void create_startup_resources();
void create_window_dependent_resources();

uint32_t create_resource_with_buffer_data(
    const char* debug_name, FG_ResourceFlags flags,
    VkBufferUsageFlags usage, size_t size, void* data
    );
PrimitiveRIDs create_primitive_resources(
    const char* debug_name, FG_ResourceFlags shared_flags, uint32_t material_index,
    uint32_t index_count, uint32_t vertex_count,
    uint32_t* indices,
    glm::vec3* positions, glm::vec2* texcoords, glm::vec3* normals, glm::vec3* colors,
    glm::uvec4* joint_ids, glm::vec4* joint_weights  // Joints only for skinned meshes
    );
// MeshRIDs create_mesh_resources(
//     const char* debug_name, FG_ResourceFlags shared_flags, 
//     );

uint32_t create_material_texture2d_resource(const char* debug_name, FG_ResourceFlags flags,
     uint8_t* data, uint64_t data_size,
     uint32_t width, uint32_t height, VkFormat format
);


// #include <bit>  // compute_num_mip_levels() uses std::countl_zero()
// // NOTE: If porting to C, C23 has stdc_leading_zeros() in <stdbit.h> header
uint32_t compute_num_mip_levels(uint32_t image_level0_width, uint32_t image_level0_height);


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
    const uint32_t MAX_RENDERED_OBJECTS = 10000;
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

    // Joints Buffer (Mapped as well as we upload them each frame)
    const uint32_t MAX_JOINTS_FOR_ALL_OBJECTS = MAX_RENDERED_OBJECTS * 50;
    ResourceCreateInfo joints_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_JOINTS_FOR_ALL_OBJECTS * sizeof(glm::mat4),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_cpu_accessible = 1  // <- Hence mapped
    };
    renderstate.rids.objects_buffer_rid = FG_CreateResource(
        "JointsBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &objects_info
    );
    renderstate.joint_transforms = MakeArenaOnBufferResource(renderstate.rids.objects_buffer_rid);


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



    /////// MOVE BELOW TO create_scene_resources(scene resource list?) /////////////
    #warning BELOW IS DUMMY DATA, THAT SHOULD NOT BE PART OF startup_resources()
    // TODO IMPORTANT: ONLY SLOT 0 OF THE MATERIAL SSBO WILL BE WRITTEN TO WITH THIS LOGIC, CHANGE THIS NEXT

    // Let's set Material 0 to be a simple White material with no texture
    MaterialData default_mat = {
        .base_color = { 1.0f, 1.0f, 1.0f, 1.0f },
        .texture_idx_basecolor = 0xFFFFFFFF,

        .sampler_idx = FG_SAMPLER_LINEAR_REPEAT,
        .alpha_cutoff = 0.5f
    };
    FG_UploadBufferData(&renderstate.main.staging_objects, 
        renderstate.rids.material_ssbo_rid, &default_mat, sizeof(MaterialData)
    );

    // TEST QUAD (TODO: Change to create_mesh_resource or something):
    uint32_t quad_indices[6] = { 0, 1, 2, 0, 3, 1 };
    glm::vec3 quad_positions[4] = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f }
    };
    glm::vec2 quad_uvs[4] = {
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f}
    };
    glm::vec3 quad_normals[4] = {
        {0,0,1},
        {0,0,1},
        {0,0,1},
        {0,0,1}
    };
    glm::vec3 quad_colors[4] = {
        {1,0,0},
        {0,1,0},
        {0,0,1},
        {0,1,1}
    };
    renderstate.rids.dummy_mesh = {
        .vertex_type = VERTEX_TYPE_STATIC,
        .mat_type    = MAT_UNLIT,
        .mesh_rids = {
            .primitive_count = 1,
            .primitives = {
                create_primitive_resources(
                    "Dummy Primitive",
                    flags,
                    0,  // material index
                    sizeof(quad_indices) / sizeof(quad_indices[0]),
                    sizeof(quad_positions) / sizeof(quad_positions[0]),
                    quad_indices, quad_positions, quad_uvs, quad_normals, quad_colors, NULL, NULL
                )
            }
        }
    };
    //  = create_mesh_resources("QuadMesh", flags, 6, 4, quad_indices,
    //     quad_positions, quad_uvs, quad_normals, quad_colors, NULL, NULL
    // );

    // Primitive* test_prim = &renderstate.temp_test_mesh->primitives[0];
    // float* test_colors = (float*)L_calloc(test_prim->vertex_count, 3 * sizeof(float) * test_prim->vertex_count, &renderstate.main.tt);
    // for (int i = 0; i < test_prim->vertex_count * 3; ++i) test_colors[i] = fabsf(sinf((float)i));
    // renderstate.rids.temp_test_mesh = create_mesh_resources(renderstate.temp_test_mesh->name, flags,
    //     test_prim->index_count, test_prim->vertex_count, test_prim->indices,
    //     (glm::vec3*)test_prim->positions, (glm::vec2*)test_prim->texcoords, (glm::vec3*)test_prim->normals,
    //     (glm::vec3*)test_colors, NULL, NULL
    // );
    // L_free(test_colors, &renderstate.main.tt);

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

uint32_t create_resource_with_buffer_data(const char* debug_name, FG_ResourceFlags flags, VkBufferUsageFlags usage, size_t size, void* data)
{
    ResourceCreateInfo res_create_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage
        }
    };
    uint32_t rid = FG_CreateResource(debug_name, FG_RESOURCE_TYPE_BUFFER, flags, &res_create_info);
    FG_UploadBufferData(&renderstate.main.staging_objects, rid, data, size);

    return rid;
}

// MeshBufferRIDs create_mesh_resources(const char* debug_name, FG_ResourceFlags shared_flags, uint32_t index_count, uint32_t vertex_count,
//     uint32_t* indices,
//     glm::vec3* positions, glm::vec2* texcoords, glm::vec3* normals, glm::vec3* colors,
    // glm::uvec4* joint_ids, glm::vec4* joint_weights)
PrimitiveRIDs create_primitive_resources(
    const char* debug_name, FG_ResourceFlags shared_flags, uint32_t material_index,
    uint32_t index_count, uint32_t vertex_count,
    uint32_t* indices,
    glm::vec3* positions, glm::vec2* texcoords, glm::vec3* normals, glm::vec3* colors,
    glm::uvec4* joint_ids, glm::vec4* joint_weights  // Joints only for skinned meshes
)
{
    // Required vertex attributes
    SDL_assert(positions && texcoords && normals);

    // Each buffer has the same usage flags
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT;


    PrimitiveRIDs prim_rids = {};
    prim_rids.material_index = material_index;

    char debug_resource_name[256] = {};

    // Index buffer
    snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_IndexBuffer", debug_name);
    prim_rids.index_buf_rid = create_resource_with_buffer_data(
        debug_resource_name, shared_flags, usage, index_count * sizeof(uint32_t), indices
    );

    // Positions
    snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_PositionBuffer", debug_name);
    prim_rids.v_pos_buf_rid = create_resource_with_buffer_data(
        debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::vec3), positions
    );

    // Texcoords
    snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_TexcoordBuffer", debug_name);
    prim_rids.v_texcoord_buf_rid = create_resource_with_buffer_data(
        debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::vec2), texcoords
    );
    
    // Normals
    snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_NormalBuffer", debug_name);
    prim_rids.v_normal_buf_rid = create_resource_with_buffer_data(
        debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::vec3), normals
    );

    // Colors (Optional) TODO: Probably remove
    prim_rids.v_color_buf_rid = UINT32_MAX;
    if (colors)
    {
        snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_ColorBuffer", debug_name);
        prim_rids.v_color_buf_rid = create_resource_with_buffer_data(
            debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::vec3), colors
        );
    }

    // Joint IDs (Optional)
    prim_rids.v_joint_ids_buf_rid = UINT32_MAX;
    if (joint_ids)
    {
        snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_JointIDsBuffer", debug_name);
        prim_rids.v_joint_ids_buf_rid = create_resource_with_buffer_data(
            debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::uvec4), joint_ids
        );
    }

    // Joint weights (Optional)
    prim_rids.v_joint_weights_buf_rid = UINT32_MAX;
    if (joint_weights)
    {
        snprintf(debug_resource_name, sizeof(debug_resource_name), "%s_JointWeights", debug_name);
        prim_rids.v_joint_weights_buf_rid = create_resource_with_buffer_data(
            debug_resource_name, shared_flags, usage, vertex_count * sizeof(glm::vec4), joint_weights
        );
    }

    return prim_rids;
}

uint32_t compute_num_mip_levels(uint32_t image_level0_width, uint32_t image_level0_height)
{
    uint32_t bits = image_level0_width | image_level0_height;

    uint32_t leading_zeros = 0;

    if (bits == 0)
        return 0;  // Edge case, though shouldn't happen for valid textures

    // Count leading zeros the silly way
    for (int i = 31; i >= 0; --i)
    {
        if (bits & (1u << i))
            break;
        leading_zeros++;
    }

    return 32 - leading_zeros;

#if 0  // NOTE(Liam): std::countl_zero not working on Jaime's machines at the moment

    // Counting number of mipmaps an image needs
    //
    // Let N := Num mip levels.
    // Let L := max(width, height) of base image (level 0).
    // Each mip level is half resolution of previous one, e.g.:
    // - Mip level 0: length = L    = L / (2^0)
    // - Mip level 1: length = L/2  = L / (2^1)
    // - Mip level 2: length = L/4  = L / (2^2)
    // - Mip level 3: length = L/8  = L / (2^3)
    //
    // Highest mip level has length of 1 pixel so num mip levels N is:
    //    N = 1 + floor( log2( max(width, height) ) )
    // => N = 1 + position of most significant bit set in max(width, height)
    // For 32 bit integers:
    // => N = 1 + (32 - (num leading zeroes + 1))
    // => N = 32 - num leading zeroes

    const uint32_t bits = image_level0_width | image_level0_height;
    const uint32_t  leading_zeros = std::countl_zero(bits);  // C++
    // const u32  leading_zeros = stdc_leading_zeros(bits);  // C23
    return 32 - leading_zeros;
#endif
}


uint32_t create_material_texture2d_resource(const char* debug_name, FG_ResourceFlags flags,
     uint8_t* data, uint64_t data_size,
     uint32_t width, uint32_t height, VkFormat format
)
{
    uint32_t miplevel_count = compute_num_mip_levels(width, height);

    ResourceCreateInfo texture_create_info = {
        .image_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,

            .extent = {
                .width  = width,
                .height = height,
                .depth  = 1
            },

            .mipLevels = miplevel_count,
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

            // These normally won't be part of the framegraph's input and outputs
            // So we set it's initial (and usually final-) image layout to read only.
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        .image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,

            .image = VK_NULL_HANDLE,  // <- This gets set in the registry code in FG_CreateResource
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,

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
    uint32_t texture_rid = FG_CreateResource(debug_name, FG_RESOURCE_TYPE_IMAGE, flags, &texture_create_info);

    // TODO: Upload image, create mipmaps return rid
    #warning create_material_texture2d_resource unfinished
    return texture_rid;
}
