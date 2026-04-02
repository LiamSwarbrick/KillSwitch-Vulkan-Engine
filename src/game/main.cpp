#include "core/core.h"
#include "renderer/renderer.h"
#include "foundations/scene.h"
#include "core/components.h"

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

    // Load test scene (This would normally happen after renderer init, but for initial test, we don't)
    //   Realistically, the splash screen assets would load first, then the main menu assets
    //   And while the user is on the main menu, we are loading the prefabs.
    //   That way, we can hide ALL of the latency and it will seem like there are no loading screens at all.
	//Asset* asset1 = load_asset("assets/levels/shapes.gltf");
    //Asset* asset2 = load_asset("assets/props/cube.gltf");
    Asset* asset3 = load_asset("assets/animations/Animationtest.gltf");
    SDL_Log("Asset 3 Extras: %s\n", asset3->nodes[0].extras_json);

    for (int i = 0; i < asset3->meshes[0].primitive_count; ++i)
    {
        SDL_Log("Mesh 0 prim %d has %zu indices\n", i, asset3->meshes[0].primitives[i].index_count);
    }

    Renderer_InitInfo renderer_info = { .window = window, .enable_validation = is_debugging };
    Renderer_Init(&renderer_info);

    // Testing Scene and ECS
    Scene scene;
    scene.LoadLevel("assets/levels/untitled.gltf");

    // Animation test
    // Find how many joints the zombie has
    uint32_t zombie_joint_count = 0;
    if (asset3->skin_count > 0) {
        zombie_joint_count = asset3->skins[0].joint_count;
    }
    else {
        zombie_joint_count = 1; 
    }

    C_AnimatedMesh temp_animated_mesh = {
        .mesh = &asset3->meshes[3],
        .parent_asset = asset3,
        .joint_count = zombie_joint_count,
        .joint_matrices = (glm::mat4*)malloc(zombie_joint_count * sizeof(glm::mat4))
    };

    for (uint32_t j = 0; j < zombie_joint_count; j++) {
        temp_animated_mesh.joint_matrices[j] = glm::mat4(1.0f);
    }

    Scene_InitInfo splash_screen_info = {
        .num_static_meshes = 0,
        .static_meshes = NULL,
        .num_animated_meshes = 1,
        .animated_meshes = &temp_animated_mesh
    };
    Renderer_ChangeScene(splash_screen_info);

    bool running = true;

    // Set up the time tracker
    uint64_t last_time = SDL_GetTicksNS();

    while (running)
    {
		// delta time calculation for testing
        uint64_t current_time = SDL_GetTicksNS();
        float dt = (float)(current_time - last_time) / 1000000000.0f;
        last_time = current_time;
        if (dt > 0.1f) dt = 0.1f;

        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            Renderer_ListenToWindowEvent(event);
        }

        // Game ticks
        // TODO:
        scene.Update(dt);

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            // Do this buddo:
            // Renderer_PushRenderable(renderable);

            Renderable r = {
                .transform = glm::mat4(1.0f),
                .mesh_prefab = temp_animated_mesh.renderer_prefab,
                .joint_count = temp_animated_mesh.joint_count,
                .joints = temp_animated_mesh.joint_matrices
            };
            Renderer_PushRenderable(r);

            Renderer_DrawFrame();
        }
    }

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
