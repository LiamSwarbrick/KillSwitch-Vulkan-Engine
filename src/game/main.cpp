#include "core/core.h"
#include "renderer/renderer.h"
// #include "foundations/scene.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

uint32_t num_renderables;
Renderable renderables_arena[MAX_RENDERED_OBJECTS];

int main(int argc, char *argv[])
{
    bool is_debugging = true;
#ifdef NDEBUG
    is_debugging = false;
#endif

    SDL_Window* window = Core_Init((Core_InitInfo){
        "Close-quarters Adventure Game",
        1280, 720
    });

    // Load test scene (This would normally happen after renderer init, but for initial test, we don't)
    //   Realistically, the splash screen assets would load first, then the main menu assets
    //   And while the user is on the main menu, we are loading the prefabs.
    //   That way, we can hide ALL of the latency and it will seem like there are no loading screens at all.
	//Asset* asset1 = load_asset("assets/levels/shapes.gltf");
    //Asset* asset2 = load_asset("assets/props/cube.gltf");
    Asset* asset3 = load_asset("assets/animations/ExtrasTest.gltf");
    SDL_Log("Asset 3 Extras: %s\n", asset3->nodes[0].extras_json);

    for (int i = 0; i < asset3->meshes[0].primitive_count; ++i)
    {
        SDL_Log("Mesh 0 prim %d has %zu indices\n", i, asset3->meshes[0].primitives[i].index_count);
    }
    Mesh* test_mesh = &asset3->meshes[1];
    

    // TODO: This will happen before asset loading, but we need to temporarilly pass the mesh to init info
    Renderer_InitInfo renderer_info = { .window = window, .enable_validation = is_debugging };
    Renderer_Init(&renderer_info);
    
    // Testing shader upload api.
    // const uint32_t* test_vert_spv = {};    
    // uint16_t shader_id = Renderer_RegisterShaders()
	//Asset* skeleton_asset = load_asset("assets/animations/ExtrasTest.gltf");
 //   if (skeleton_asset) {
 //       SDL_Log("=== Skeleton Asset Debug Info ===");

 //       // Debug Skins & Bones
 //       SDL_Log("Skins: %zu", skeleton_asset->skin_count);
 //       for (size_t i = 0; i < skeleton_asset->skin_count; ++i) {
 //           Skin* skin = &skeleton_asset->skins[i];
 //           SDL_Log("  Skin %zu: '%s', Root Node: %d, Joints: %zu",
 //               i, skin->name ? skin->name : "Unnamed", skin->skeleton_root_node_index, skin->joint_count);

 //           for (size_t j = 0; j < skin->joint_count; ++j) {
 //               Bone* bone = &skin->bones[j];
 //               SDL_Log("    Bone %zu: '%s', Parent: %d, Children: %zu",
 //                   j, bone->name ? bone->name : "Unnamed", bone->parent_index, bone->child_count);
 //               SDL_Log("      Translation: [%.3f, %.3f, %.3f]",
 //                   bone->translation[0], bone->translation[1], bone->translation[2]);
 //           }
 //       }

 //       // Debug Meshes & Skinning Data
 //       SDL_Log("Meshes: %zu", skeleton_asset->mesh_count);
 //       for (size_t i = 0; i < skeleton_asset->mesh_count; ++i) {
 //           Mesh* mesh = &skeleton_asset->meshes[i];
 //           SDL_Log("  Mesh %zu: '%s', Type: %d, Primitives: %zu",
 //               i, mesh->name ? mesh->name : "Unnamed", mesh->type, mesh->primitive_count);

 //           for (size_t j = 0; j < mesh->primitive_count; ++j) {
 //               Primitive* prim = &mesh->primitives[j];
 //               SDL_Log("    Primitive %zu: Vertices: %zu, Indices: %zu, Has Joints: %s, Has Weights: %s",
 //                   j, prim->vertex_count, prim->index_count,
 //                   prim->joints ? "Yes" : "No",
 //                   prim->weights ? "Yes" : "No");
 //           }
 //       }

 //       // Debug Animations
 //       SDL_Log("Animations: %zu", skeleton_asset->animation_count);
 //       for (size_t i = 0; i < skeleton_asset->animation_count; ++i) {
 //           Animation* anim = &skeleton_asset->animations[i];
 //           SDL_Log("  Animation %zu: '%s', Samplers: %zu, Channels: %zu",
 //               i, anim->name ? anim->name : "Unnamed", anim->sampler_count, anim->channel_count);

 //           for (size_t j = 0; j < anim->channel_count; ++j) {
 //               AnimationChannel* channel = &anim->channels[j];
 //               const char* target_path_str = "Unknown";
 //               switch (channel->target_path) {
 //               case 0: target_path_str = "Translation"; break;
 //               case 1: target_path_str = "Rotation"; break;
 //               case 2: target_path_str = "Scale"; break;
 //               case 3: target_path_str = "Weights"; break;
 //               }
 //               SDL_Log("    Channel %zu: Target Node Index: %d, Target Path: %d (%s)",
 //                   j, channel->target_node_index, channel->target_path, target_path_str);
 //           }
 //       }
 //       SDL_Log("=================================");
 //   }


    // Testing Scene and ECS
    // Scene scene;
    // scene.LoadLevel("assets/levels/shapes.gltf");

    //


    Scene_InitInfo splash_screen_info = {
        .num_prefabs = 1,
        .prefabs = &asset3
    };
    Renderer_ChangeScene(splash_screen_info);


    bool running = true;

    while (running)
    {
        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            Renderer_ListenToWindowEvent(event);
        }

        // Game ticks
        // TODO:

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            // Do this buddo:
            // Renderer_PushRenderable(renderable);

            Renderer_DrawFrame();
        }
    }

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
