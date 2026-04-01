#include "game_resources.h"

#include "internal_state.h"

#include "stb_image.h"
#include "core/assetsys.h"  // CPU-side asset data

void create_startup_resources();
void create_window_dependent_resources();
void create_scene_resources();

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
uint32_t create_mipmapped_texture2d_resource(const char* debug_name, FG_ResourceFlags flags,
     const uint8_t* data, uint64_t data_size,
     uint32_t width, uint32_t height, VkFormat format
);

// NOTE: Not using an optimized built-in for count leading zeros
// because mipmaps level counting is not a bottleneck and Jaime was having problems with
// C++20 features.
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

    flags = FG_RESOURCE_FLAGS_SCENE_DEPENDENT;
    if ((flags & types_to_create) == types_to_create)
    {
        create_scene_resources();
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

    // Joints Buffer (Mapped so we upload to them each frame)
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


    // Materials SSBO (materials get uploaded on scene change all at once)
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


    // DUMMY DATA BELOW:
    #warning TODO: Upload materials on create_scene_resources() instead of startup.

#if 0
    // TEMP Material:
    uint32_t test_texture_rid = UINT32_MAX;
    {
        stbi_set_flip_vertically_on_load(1);
        int width, height, num_channels;
        const char* filepath = "assets/godot.png";
        uint8_t* data = stbi_load(filepath, &width, &height, &num_channels, 4);
        if (data == NULL)
        {
            fprintf(stderr, "Failure to load image (%s)\n", filepath);
            exit(1);  // TODO: Fallback to default texture
        }
        uint64_t data_size = width * height * 4;
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        test_texture_rid = create_mipmapped_texture2d_resource(filepath, flags, data, data_size, width, height, format);
        
        stbi_image_free(data);
    }
    MaterialData default_mat = {
        .base_color = { 1.0f, 1.0f, 1.0f, 1.0f },
        .texture_idx_basecolor = renderstate.registry.resources[test_texture_rid].image_bindless_index,//0xFFFFFFFF,

        .sampler_idx = FG_SAMPLER_LINEAR_REPEAT,
        .alpha_cutoff = 0.5f
    };

    const uint32_t temp_max_materials = 32;
    MaterialData materials[temp_max_materials] = {
        default_mat  // index 0
    };
    FG_UploadBufferData(&renderstate.main.staging_objects, 
        renderstate.rids.material_ssbo_rid, &materials, sizeof(materials)
    );
#endif

    /////// MOVE BELOW TO create_scene_resources(scene resource list?) /////////////

    // TEST QUAD
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
                    0,  // material index (TODO: Load all materials on scene change, and keep track of material indices CPU side)
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

void create_scene_resources()
{
    FG_ResourceFlags flags = FG_RESOURCE_FLAGS_SCENE_DEPENDENT;

    SDL_assert(renderstate.is_next_scene_set);
    renderstate.is_next_scene_set = 0;

    // Load next scene
    Scene_InitInfo* init_info = &renderstate.next_scene_info;
    ResourceIDs* rids = &renderstate.rids;

    /*
        NOTES For 1st April:
        Only need to worry about:
        - Static Mesh Data (the joints are passed by the animation system)
        - Gather vertextype, e.g. static vs skinned from custom properties?

        - Load textures as resources (automatically gives them an rid in res->image_bindless_index)
        - Get material data
    
    */

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: Account for animated meshes as well                                                     //
    // TODO: Create stylised gradients via colour buffer for characters meshes (i.e. skinned meshes) //
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    // Get the list of unique assets (since meshes can be from the same parent asset)
    uint32_t num_unique_assets = 0;
    const uint32_t max_assets = 1024;  // <- Arbitrary. If this is too low, L_calloc a bigger amount
    Asset*   unique_assets[max_assets] = {};
    uint32_t material_count = 0;

    for (uint32_t i = 0; i < init_info->num_static_meshes; ++i)
    {
        C_StaticMesh* component = &init_info->static_meshes[i];

        for (uint32_t j = 0; j < num_unique_assets; ++j)
        {
            if (unique_assets[j] == component->parent_asset)
            {
                goto seen_this_asset_before;
            }
        }

        SDL_assert(num_unique_assets < max_assets && 
            "If this is too low, L_calloc a bigger amount instead of the stack allocator currently used."
        );
        unique_assets[num_unique_assets++] = component->parent_asset;
        material_count += component->parent_asset->material_count;

    seen_this_asset_before:
    }

    // Load unique materials
    uint32_t num_loaded_materials = 0;
    MaterialData* loaded_materials = (MaterialData*)L_calloc(material_count, sizeof(MaterialData), &renderstate.main.tt);
    uint32_t assets_mat_start_idx[max_assets] = {};
    memset(assets_mat_start_idx, 0xFFFF, max_assets);

    for (uint32_t i = 0; i < num_unique_assets; ++i)
    {
        Asset* asset = unique_assets[i];
        assets_mat_start_idx[i] = num_loaded_materials;

        for (uint32_t j = 0; j < unique_assets[i]->material_count; ++j)
        {
            Material* mat = &asset->materials[j];

            MaterialData gpu_mat = {};
            memcpy(&gpu_mat.base_color, mat->base_color, sizeof(glm::vec4));
            gpu_mat.metalness = mat->metallic;
            gpu_mat.roughness = mat->roughness;
            memcpy(&gpu_mat.emissive_factor, mat->emissive_factor, sizeof(glm::vec3));
            gpu_mat.alpha_cutoff = mat->alpha_cutoff;

            // NOTE: Just use one sampler for now at least
            // (maybe I'd want to chec the base colour texture's min/mag filter and s/t wrap to choose a better one if we need)
            gpu_mat.sampler_idx = FG_SAMPLER_LINEAR_REPEAT;
            
            if (mat->base_color_texture_index >= 0)
            {
                Texture* base_color_texture = &asset->textures[mat->base_color_texture_index];
                Image*   base_color_image   = &asset->images[base_color_texture->image_index];

                // Create texture resource
                gpu_mat.texture_idx_basecolor = create_mipmapped_texture2d_resource(
                    base_color_texture->name, flags, base_color_image->data, base_color_image->data_size,
                    base_color_image->width, base_color_image->height,
                    VK_FORMAT_R8G8B8A8_SRGB  // <- is a colour texture
                );
            }
            else
            {
                gpu_mat.texture_idx_basecolor = UINT32_MAX;
            }

            loaded_materials[num_loaded_materials++] = gpu_mat;
        }
    }

    // Upload materials to global material buffer (all at once)
    FG_UploadBufferData(&renderstate.main.staging_objects, rids->material_ssbo_rid,
        loaded_materials, num_loaded_materials * sizeof(MaterialData)
    );


    // Load meshes
    for (uint32_t i = 0; i < init_info->num_static_meshes; ++i)
    {
        C_StaticMesh* component = &init_info->static_meshes[i];

        component->renderer_prefab = {
            .vertex_type = component->mesh->vertex_type,
            .mat_type    = component->mesh->mat_type,
            .mesh_rids   = {
                .primitive_count = (uint32_t)component->mesh->primitive_count
            }
        };

        uint32_t asset_idx = UINT32_MAX;
        for (uint32_t a = 0; a < num_unique_assets; ++a)
        {
            if (component->parent_asset == unique_assets[a])
            {
                asset_idx = a;
            }
        }
        SDL_assert(asset_idx < UINT32_MAX);
        uint32_t mat_start_idx = assets_mat_start_idx[asset_idx];

        // Load primitives into GPU resources
        char prim_resource_debug_name[256] = {};
        for (uint32_t p = 0; p < component->mesh->primitive_count; ++p)
        {
            Primitive* prim = &component->mesh->primitives[p];
            uint32_t gpu_mat_idx = mat_start_idx + prim->material_index;

            snprintf(prim_resource_debug_name, sizeof(prim_resource_debug_name),
                "%s_Prim%u", component->mesh->name, p
            );
            component->renderer_prefab.mesh_rids.primitives[p] = create_primitive_resources(
                prim_resource_debug_name, flags,
                gpu_mat_idx,
                prim->index_count, prim->vertex_count,
                prim->indices, (glm::vec3*)prim->positions,
                (glm::vec2*)prim->texcoords, (glm::vec3*)prim->normals,
                NULL, (glm::uvec4*)prim->joints, (glm::vec4*)prim->weights
            );
        }
    }

    L_free(loaded_materials, &renderstate.main.tt);
}

/////

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
}


uint32_t create_mipmapped_texture2d_resource(const char* debug_name, FG_ResourceFlags flags,
     const uint8_t* data, uint64_t data_size,
     uint32_t width, uint32_t height, VkFormat format)
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
            .queueFamilyIndexCount = 0,  // <- Zero when using exclusive sharing mode
            .pQueueFamilyIndices = NULL,

            // Mipmapped textures normally won't be part of the framegraph's input and outputs
            // So we set it's initial (and presumably final-) image layout to read only.
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
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
    FG_UploadImageData(&renderstate.main.staging_objects, texture_rid, data, data_size);
    FG_GenMipmaps(texture_rid);

    return texture_rid;
}
