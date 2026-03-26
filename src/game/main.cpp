#include "core/core.h"
#include "renderer/renderer.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

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
            // NOTE: Passes will gather their own renderables, reason being, some passes will just draw a hardcoded full screen triangle,
            // others will draw from the lights perspective, and others will only draw the toon shaded characters for example
            //
            // TODO: Gather entity renderables in RenderView, put this as a function in renderer/renderpasses/gather_renderables.cpp or something
            // Later TODO: Gather visible entities only for extra optimization.
            // with support for different passes e.g. shadows from lights perspective will require different entity lists.
            // In future, when entity system sorted out, can move entity gathering
            // to inside the Renderer_DrawFrame function, and this can use core
            // systems to gather relevant entities per each render pass.
            
            Renderer_DrawFrame();
        }
    }

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
