#include "game_resources.h"

#include "internal_state.h"

#include "stb_image.h"
#include "core/assetsys.h"  // CPU-side asset data

#include "stb_image.h"

void create_startup_resources();
void create_window_dependent_resources();
void create_scene_resources();

uint32_t create_resource_with_buffer_data(const char* debug_name, FG_ResourceFlags flags, VkBufferUsageFlags usage, size_t size, void* data);
PrimitiveRIDs create_primitive_resources(
    const char*       debug_name,
    FG_ResourceFlags  shared_flags,
    uint32_t          material_index,
    uint32_t          index_count,
    uint32_t          vertex_count,
    uint32_t*         indices,
    glm::vec3*        positions,
    glm::vec2*        texcoords,
    glm::vec3*        normals,
    glm::vec3*        colors,
    glm::uvec4*       joint_ids,
    glm::vec4*        joint_weights
);
uint32_t compute_num_mip_levels(uint32_t image_level0_width, uint32_t image_level0_height);
uint32_t create_mipmapped_texture2d_resource(
    const char*       debug_name,
    FG_ResourceFlags  flags,
    const uint8_t*    data,
    uint64_t          data_size,
    uint32_t          width,
    uint32_t          height, 
    VkFormat          format
);
uint32_t create_rendertarget2d_resource(
    const char*            debug_name,
    FG_ResourceFlags       flags,
    uint32_t               width,
    uint32_t               height,
    VkFormat               format,
    VkImageAspectFlags     aspect,
    b32                    multisampled,
    b32                    is_transient
);


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
                #ifdef VERBOSE_FRAMEGRAPH_LOGGING
                printf("Deallocating resource: %s\n", res->debug_name);
                #endif
                FG_DeallocateResource(res);
            }
        }
    }

    
    // The request resource types have now been emptied
    // So we recreate them...

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
    // NOTE: One SceneData per renderpass so there's no worrying about synchronisation to update the scene data between renderpasses.
    //       Index scene buffer with the pass_idx!
    ResourceCreateInfo scene_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_PASSES * PaddedSizeForMappedArena(sizeof(SceneData)),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1  // <- Mapped bcuz small data upload is most efficient this way
    };
    renderstate.rids.scenes_buffer_rid = FG_CreateResource(
        "GlobalSceneBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &scene_info
    );
    renderstate.scenes_arena = MakeArenaOnBufferResource(renderstate.rids.scenes_buffer_rid);


    // Objects Buffer (Mapped so we rapidly upload transforms each frame)
    ResourceCreateInfo objects_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_RENDERED_OBJECTS * PaddedSizeForMappedArena(sizeof(ObjectData)),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1  // <- Hence mapped
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
            // NOTE: Each objects joint transforms are contiguous. But subsequent objects are pushed with seperate PushToMappedArena calls.
            // Obviously mat4 is 64 byte aligned so there is technically no padding between adjacent object's joint arrays.
            // But just to be clear I've included the differences between different object's joint arrays 
            // (but Padded(mat4)-mat4 = 0 so it cancel out due to 64 byte alignment and 64 byte size of mat4)
            .size = MAX_JOINTS_FOR_ALL_OBJECTS * sizeof(glm::mat4) + MAX_RENDERED_OBJECTS*(PaddedSizeForMappedArena(sizeof(glm::mat4))-sizeof(glm::mat4)),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1  // <- Hence mapped
    };
    renderstate.rids.joints_buffer_rid = FG_CreateResource(
        "JointsBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &objects_info
    );
    renderstate.joint_transforms = MakeArenaOnBufferResource(renderstate.rids.joints_buffer_rid);


    // Materials SSBO (materials get uploaded on scene change all at once)
    ResourceCreateInfo mat_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(MaterialData) * MAX_MATERIALS,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        },
        .is_buffer_cpu_accessible = 1  // <- Material stuff like blend mode is important to be CPU accessable, and we may want to dynamically change materials.
    };
    renderstate.rids.materials_buffer_rid = FG_CreateResource(
        "MaterialBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &mat_info
    );

    // Light Buffers (Uploaded once per frame all at once) (packed arrays)
    ResourceCreateInfo lights_header_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = sizeof(LightsHeader),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1
    };
    renderstate.rids.lights_header_buffer_rid = FG_CreateResource(
        "LightsHeaderBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &lights_header_info
    );

    ResourceCreateInfo point_lights_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = sizeof(PointLight) * MAX_POINTLIGHTS,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1
    };
    renderstate.rids.point_lights_buffer_rid = FG_CreateResource(
        "PointLightsBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &point_lights_info
    );

    ResourceCreateInfo spot_lights_info = {
        .buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = sizeof(SpotLight) * MAX_SPOTLIGHTS,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        },
        .is_buffer_cpu_accessible = 1
    };
    renderstate.rids.spot_lights_buffer_rid = FG_CreateResource(
        "SpotLightsBuffer", FG_RESOURCE_TYPE_BUFFER, flags, &spot_lights_info
    );


#if 0  // NOTE: Keeping in case it's useful for game ui to have quad code lying around
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
                    0,  // Default material index
                    sizeof(quad_indices) / sizeof(quad_indices[0]),
                    sizeof(quad_positions) / sizeof(quad_positions[0]),
                    quad_indices, quad_positions, quad_uvs, quad_normals, quad_colors, NULL, NULL
                )
            }
        }
    };
#endif
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
                .extent  = (VkExtent3D){ renderstate.swapchain_extent.width, renderstate.swapchain_extent.height, 1 },
                .usage   = renderstate.swapchain_usage,
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

    const uint32_t width = renderstate.swapchain_extent.width;
    const uint32_t height = renderstate.swapchain_extent.height;
    const VkFormat HDR_COLOR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    // Depth buffer
    renderstate.rids.depth_buffer_rid = create_rendertarget2d_resource(
        "Depth Buffer", flags, width, height,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        1, 0
    );

    // Foward render target
    renderstate.rids.forward_target_rid = create_rendertarget2d_resource(
        "Forward Render Target", flags, width, height,
        HDR_COLOR_FORMAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1, 1
    );

    // Resolve HDR color target for post processing/deferred steps
    if (renderstate.multisampling_count_flag > VK_SAMPLE_COUNT_1_BIT)
    {
        renderstate.rids.hdr_color_target_rid = create_rendertarget2d_resource(
            "HDR Color Target", flags, width, height,
            HDR_COLOR_FORMAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 0
        );
    }
    else
    {
        // Alias to forward render target when not using MSAA
        renderstate.rids.hdr_color_target_rid = renderstate.rids.forward_target_rid;
    }

    // HDR color ping pong buffer for bloom
    // renderstate.rids.hdr_color_target_pingpong_rid = create_rendertarget2d_resource(
    //     "Forward Render Target", flags, width, height,
    //     HDR_COLOR_FORMAT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     0, 0
    // );

    // LDR color ping pong buffer for bloom
    renderstate.rids.ldr_color_target_rid = create_rendertarget2d_resource(
        "LDR Color Target", flags, width, height,
        renderstate.swapchain_image_format,  // <- TODO: Is matching the swapchain image always best? I.e. sRGB?
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 0
    );
}

void create_scene_resources()
{
    FG_ResourceFlags flags = FG_RESOURCE_FLAGS_SCENE_DEPENDENT;

    SDL_assert(renderstate.is_next_scene_set);
    renderstate.is_next_scene_set = 0;

    // Load next scene
    Scene_InitInfo* init_info = &renderstate.next_scene_info;
    ResourceIDs* rids = &renderstate.rids;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: Create stylised gradients via colour buffer for characters meshes (i.e. skinned meshes) //
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    // Get the list of unique assets (since meshes can be from the same parent asset)
    uint32_t num_unique_assets = 0;
    const uint32_t max_assets = 1024;  // <- Arbitrary. If this is too low, L_calloc a bigger amount
    Asset*   unique_assets[max_assets] = {};
    uint32_t material_count = 1;  // Start at one to give space for default material

    for (uint32_t i = 0; i < init_info->num_static_meshes; ++i)
    {
        C_StaticMesh* component = init_info->static_meshes[i];

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

    // Also count animated meshes
    for (uint32_t i = 0; i < init_info->num_animated_meshes; ++i)
    {
        C_AnimatedMesh* component = init_info->animated_meshes[i];

        for (uint32_t j = 0; j < num_unique_assets; ++j)
        {
            if (unique_assets[j] == component->asset)
            {
                goto seen_this_anim_asset_before;
            }
        }

        SDL_assert(num_unique_assets < max_assets);
        unique_assets[num_unique_assets++] = component->asset;
        material_count += component->asset->material_count;

    seen_this_anim_asset_before:
    }

    // Load unique materials
    uint32_t num_loaded_materials = 0;
    MaterialData* loaded_materials = (MaterialData*)L_calloc(material_count, sizeof(MaterialData), &renderstate.main.tt);
    uint32_t assets_mat_start_idx[max_assets] = {};
    memset(assets_mat_start_idx, 0xFFFF, max_assets);

    // (First add default material)
    {
        uint32_t default_texture_rid = UINT32_MAX;
        stbi_set_flip_vertically_on_load(1);
        int width, height, num_channels;
        const char* filepath = "assets/godot2.png";
        uint8_t* data = stbi_load(filepath, &width, &height, &num_channels, 4);
        if (data == NULL)
        {
            fprintf(stderr, "Failure to load image (%s)\n", filepath);
            exit(1);  // TODO: Fallback to default texture
        }
        uint64_t data_size = width * height * 4;
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        default_texture_rid = create_mipmapped_texture2d_resource(filepath, flags, data, data_size, width, height, format);
        stbi_image_free(data);
        
        MaterialData default_mat = {
            .base_color = { 1.0f, 1.0f, 1.0f, 1.0f },
            .blend_mode = BLEND_MODE_MASKED,
            .alpha_cutoff = 0.5f,
            .sampler_idx = FG_SAMPLER_ANISOTROPIC_REPEAT,
            .texture_idx_basecolor = renderstate.registry.resources[default_texture_rid].bindless_texture_idx,
            .texture_idx_emissive = UINT32_MAX
        };
        loaded_materials[num_loaded_materials++] = default_mat;
    }

    // (Then Load the actual assets in the scene)
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
            gpu_mat.blend_mode = mat->blend_mode;
            gpu_mat.alpha_cutoff = mat->alpha_cutoff;

            // NOTE: Just use one sampler for now at least
            // (maybe I'd want to check the base colour texture's min/mag filter and s/t wrap to choose a better one if we need)
            gpu_mat.sampler_idx = FG_SAMPLER_ANISOTROPIC_REPEAT;
            
            // BASECOLOR TEXTURE
            if (mat->base_color_texture_index >= 0)
            {
                Texture* base_color_texture = &asset->textures[mat->base_color_texture_index];
                Image*   base_color_image   = &asset->images[base_color_texture->image_index];

                // Create texture resource
                uint32_t new_texture_rid = create_mipmapped_texture2d_resource(
                    base_color_image->uri, flags, base_color_image->data, base_color_image->data_size,
                    base_color_image->width, base_color_image->height,
                    VK_FORMAT_R8G8B8A8_SRGB  // <- is a colour texture
                );
                gpu_mat.texture_idx_basecolor = renderstate.registry.resources[new_texture_rid].bindless_texture_idx;
            }
            else
            {
                gpu_mat.texture_idx_basecolor = UINT32_MAX;
            }

            // EMISSIVE TEXTURE
            if (mat->emissive_texture_index >= 0)
            {
                Texture* emissive_texture = &asset->textures[mat->emissive_texture_index];
                Image*   emissive_image   = &asset->images[emissive_texture->image_index];

                // Create texture resource
                uint32_t new_texture_rid = create_mipmapped_texture2d_resource(
                    emissive_image->uri, flags, emissive_image->data, emissive_image->data_size,
                    emissive_image->width, emissive_image->height,
                    VK_FORMAT_R8G8B8A8_SRGB  // <- is a colour texture
                );
                gpu_mat.texture_idx_emissive = renderstate.registry.resources[new_texture_rid].bindless_texture_idx;
            }
            else
            {
                gpu_mat.texture_idx_emissive = UINT32_MAX;
            }

            loaded_materials[num_loaded_materials++] = gpu_mat;
        }
    }

    // Upload materials to global material buffer (all at once)
    FG_Resource* materials_res = &renderstate.registry.resources[renderstate.rids.materials_buffer_rid];
    vmaCopyMemoryToAllocation(renderstate.vma_allocator, loaded_materials, materials_res->allocation, 0, num_loaded_materials * sizeof(MaterialData));
    // memcpy(materials_res->buffer.mapped_data, loaded_materials, mat_upload_size);
    // vmaFlushAllocation(renderstate.vma_allocator, materials_res->allocation, 0, mat_upload_size);

    // Load Static meshes
    for (uint32_t i = 0; i < init_info->num_static_meshes; ++i)
    {
        C_StaticMesh* component = init_info->static_meshes[i];

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
                NULL, NULL, NULL
            );
        }
    }

    // Load animated meshes
    for (uint32_t i = 0; i < init_info->num_animated_meshes; ++i)
    {
        C_AnimatedMesh* component = init_info->animated_meshes[i];
        
        component->renderer_prefab = {
            .vertex_type = component->mesh->vertex_type, 
            .mat_type = component->mesh->mat_type,
            .mesh_rids = {
                .primitive_count = (uint32_t)component->mesh->primitive_count
            }
        };

        uint32_t asset_idx = UINT32_MAX;
        for (uint32_t a = 0; a < num_unique_assets; ++a)
        {
            if (component->asset == unique_assets[a]) asset_idx = a;
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
    const char*       debug_name,
    FG_ResourceFlags  shared_flags,
    uint32_t          material_index,
    uint32_t          index_count,
    uint32_t          vertex_count,
    uint32_t*         indices,
    glm::vec3*        positions,
    glm::vec2*        texcoords,
    glm::vec3*        normals,
    glm::vec3*        colors,
    glm::uvec4*       joint_ids,
    glm::vec4*        joint_weights
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
    /*
        NOTE: Not using an optimized built-in for count leading zeros
        because mipmaps level counting is not a bottleneck and Jaime was having problems with
        C++20 features.
        #include <bit>  // compute_num_mip_levels() uses std::countl_zero()
        // NOTE: If porting to C, C23 has stdc_leading_zeros() in <stdbit.h> header
    */
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


uint32_t create_mipmapped_texture2d_resource(
    const char*       debug_name,
    FG_ResourceFlags  flags,
    const uint8_t*    data,
    uint64_t          data_size,
    uint32_t          width,
    uint32_t          height, 
    VkFormat          format
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
            .queueFamilyIndexCount = 0,  // <- Zero when using exclusive sharing mode
            .pQueueFamilyIndices = NULL,
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

uint32_t create_rendertarget2d_resource(
    const char*            debug_name,
    FG_ResourceFlags       flags,
    uint32_t               width,
    uint32_t               height,
    VkFormat               format,
    VkImageAspectFlags     aspect,
    b32                    multisampled,
    b32                    is_transient
)
{
    // Disable multisampling and transience when not using it
    if (renderstate.multisampling_count_flag == VK_SAMPLE_COUNT_1_BIT)
    {
        multisampled = 0;
        is_transient = 0;
    }

    b32 is_depth_stencil_attachment = (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) || (aspect & VK_IMAGE_ASPECT_STENCIL_BIT);

    VkImageUsageFlags attachment_specific_usage = is_depth_stencil_attachment ?
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (is_transient)
    {
        // MSAA images that are never stored to main memory don't even need to exist
        attachment_specific_usage |=
              VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            // | VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    else
    {
        attachment_specific_usage |=
              VK_IMAGE_USAGE_SAMPLED_BIT;
            // | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            // | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VkSampleCountFlagBits multisample_count = multisampled ?
        renderstate.multisampling_count_flag : VK_SAMPLE_COUNT_1_BIT;

    ResourceCreateInfo rendertarget_create_info = {
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

            .mipLevels = 1,
            .arrayLayers = 1,

            .samples = multisample_count,  // <- Multisampling
            .tiling = VK_IMAGE_TILING_OPTIMAL,

            .usage = attachment_specific_usage,

            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,  // <- Zero when using exclusive sharing mode
            .pQueueFamilyIndices = NULL,
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
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };
    uint32_t texture_rid = FG_CreateResource(debug_name, FG_RESOURCE_TYPE_IMAGE, flags, &rendertarget_create_info);
    return texture_rid;
}
