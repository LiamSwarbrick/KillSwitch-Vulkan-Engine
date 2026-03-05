#include "engine.h"

// TODO: The current concept of a scene might need rearchitecting for something nicer.
// Similarly to lights.cpp, the function prototypes are in engine.h and scene structs in engine_structs.h

void
destroy_scene(VulkanEngine* engine)
{
    Scene* scene = &engine->scene;
    if (!scene->is_initialised) return;
    scene->is_initialised = 0;

    // Must destroy things in reverse order

    // Destroy all statics
    if (scene->static_renderable_ids)
    {
        L_free(scene->static_renderable_ids, &engine->main.tt);
    }

    // Destroy all renderables
    if (scene->renderables)
    {
        // NOTE: Renderables don't own anything, they just references stuff.
        // so we don't destroy any objects, just free the memory
        L_free(scene->renderables, &engine->main.tt);
    }

    // Destroy all meshes
    if (scene->meshes)
    {
        for (u32 i = 0; i < scene->meshes_size; ++i)
            destroy_mesh_buffers(engine, &scene->meshes[i]);
        L_free(scene->meshes, &engine->main.tt);
    }

    // Destroy all textures
    if (scene->textures)
    {
        for (u32 i = 0; i < scene->textures_size; ++i)
            destroy_image(engine, scene->textures[i]);
        L_free(scene->textures, &engine->main.tt);
    }

    // Destroy all lights
    if (scene->point_lights)
    {
        L_free(scene->point_lights, &engine->main.tt);
    }
}

GPU_Image*
scene_add_texture(VulkanEngine* engine, GPU_Image gpu_image)
{
    Scene* scene = &engine->scene;

    // On first call to we allocate the array
    if (scene->textures == NULL)
    {
        // Allocate array with arbitrary initial capacity
        scene->textures_size = 0;
        scene->textures_capacity = 256;
        scene->textures = (GPU_Image*)L_calloc(scene->textures_capacity, sizeof(GPU_Image), &engine->main.tt);
    }

    // Resize with larger block if going over capacity
    if (scene->textures_size + 1 >= scene->textures_capacity)
    {
        scene->textures_capacity *= 2;
        scene->textures = (GPU_Image*)L_realloc(scene->textures, scene->textures_capacity, &engine->main.tt);
    }

    u32 texture_id = scene->textures_size++;
    scene->textures[texture_id] = gpu_image;

    return (GPU_Image*)&scene->textures[texture_id];
}

GPU_MeshBuffers*
scene_add_mesh(VulkanEngine* engine, GPU_MeshBuffers gpu_mesh)
{
    Scene* scene = &engine->scene;

    // On first call to we allocate the array
    if (scene->meshes == NULL)
    {
        // Allocate array with arbitrary initial capacity
        scene->meshes_size = 0;
        scene->meshes_capacity = 256;
        scene->meshes = (GPU_MeshBuffers*)L_calloc(scene->meshes_capacity, sizeof(GPU_MeshBuffers), &engine->main.tt);
    }

    // Resize with larger block if going over capacity
    if (scene->meshes_size + 1 >= scene->meshes_capacity)
    {
        scene->meshes_capacity *= 2;
        scene->meshes = (GPU_MeshBuffers*)L_realloc(scene->meshes, scene->meshes_capacity, &engine->main.tt);
    }

    u32 mesh_index = scene->meshes_size++;
    scene->meshes[mesh_index] = gpu_mesh;

    return &scene->meshes[mesh_index];
}

Renderable*
scene_add_renderable(VulkanEngine* engine, b32 is_static, Renderable renderable)
{
    Scene* scene = &engine->scene;

    // On first call to we allocate the array
    if (scene->renderables == NULL)
    {
        // Allocate array with arbitrary initial capacity
        scene->renderables_size = 0;
        scene->renderables_capacity = 256;
        scene->renderables = (Renderable*)L_calloc(scene->renderables_capacity, sizeof(Renderable), &engine->main.tt);
    }

    // Resize with larger block if going over capacity
    if (scene->renderables_size + 1 >= scene->renderables_capacity)
    {
        scene->renderables_capacity *= 2;
        scene->renderables = (Renderable*)L_realloc(scene->renderables, scene->renderables_capacity, &engine->main.tt);
    }

    u32 renderable_index = scene->renderables_size++;
    scene->renderables[renderable_index] = renderable;


    // If static, we add to the list of statics
    if (is_static)
    {
        // On first call to we allocate the array
        if (scene->static_renderable_ids == NULL)
        {
            // Allocate array with arbitrary initial capacity
            scene->statics_size = 0;
            scene->statics_capacity = 256;
            scene->static_renderable_ids = (u32*)L_calloc(scene->statics_capacity, sizeof(u32), &engine->main.tt);
        }

        // Resize with larger block if going over capacity
        if (scene->statics_size + 1 >= scene->statics_capacity)
        {
            scene->statics_capacity *= 2;
            scene->static_renderable_ids = (u32*)L_realloc(scene->static_renderable_ids, scene->statics_capacity, &engine->main.tt);
        }

        scene->static_renderable_ids[scene->statics_size++] = renderable_index;
    }

    return &scene->renderables[renderable_index];
}

PointLight*
scene_add_pointlight(VulkanEngine* engine, PointLight p)
{
    Scene* scene = &engine->scene;

    // On first call to we allocate the array
    if (scene->point_lights == NULL)
    {
        // Allocate array with arbitrary initial capacity
        scene->point_lights_size = 0;
        scene->point_lights_capacity = 256;
        scene->point_lights = (PointLight*)L_calloc(scene->point_lights_capacity, sizeof(PointLight), &engine->main.tt);
    }

    // Resize with larger block if going over capacity
    if (scene->point_lights_size + 1 >= scene->point_lights_capacity)
    {
        scene->point_lights_capacity *= 2;
        scene->point_lights = (PointLight*)L_realloc(scene->point_lights, scene->point_lights_capacity, &engine->main.tt);
    }

    u32 index = scene->point_lights_size++;
    scene->point_lights[index] = p;

    return &scene->point_lights[index];
}

void
create_scene(VulkanEngine* engine)
{
    destroy_scene(engine);
    Scene* scene = &engine->scene;

    // // Create initial textures:
    // VkImageUsageFlags texture_usage =
    //     VK_IMAGE_USAGE_SAMPLED_BIT |
    //     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    //     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    // // Default albedo alpha map:
    // scene_add_texture(engine, 
    //     create_image_texture2d(engine,
    //         (u8*)default_albedo_alpha_map, sizeof(default_albedo_alpha_map),
    //         1, 1, default_albedo_alpha_format, texture_usage)
    // );
    // // Default RMA texture (roughness, metalness, ambient occlusion)
    // scene_add_texture(engine, 
    //     create_image_texture2d(engine,
    //         (u8*)default_RMA_map, sizeof(default_RMA_map),
    //         1, 1, default_RMA_format, texture_usage)
    // );
    // // Default normal texture
    // scene_add_texture(engine, 
    //     create_image_texture2d(engine,
    //         (u8*)default_normal_map, sizeof(default_normal_map),
    //         1, 1, default_normal_format, texture_usage)
    // );
    // // Default emissive texture
    // scene_add_texture(engine, 
    //     create_image_texture2d(engine,
    //         (u8*)default_emissive_map, sizeof(default_emissive_map),
    //         1, 1, default_emissive_format, texture_usage)
    // );

    // Finally:
    scene->is_initialised = 1;

    VERBOSE_LOG("Initialised empty scene and default texture maps.\n");
}
