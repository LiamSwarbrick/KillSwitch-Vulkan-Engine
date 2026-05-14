#include "core/core.h"
#include "core/input.h"
#include "renderer/renderer.h"
#include "renderer/debug_ui_api.h"
#include "foundations/scene.h"
#include "core/components.h"
#include "core/animation.h"
#include "game_audio.h"
#include "game_ui.h"
#include "ingame_cam.h"
#include "game/foundations/components.h"
#include "physics/body_layers.h"

#include "core/utils/math_utils.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "foundations/level/LevelGeneration.h"


// Hacky fix
#define GMH_IMPL
#include "game/game_restart_hack.h"

#include "game/game_state.h"

InternalGameState gamestate = {
    .disable_hud = 0,
    .num_zombies_killed = 0
};

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bool enabled_validation_layers = false;
    // NOTE: For non production builds, we still want Vulkan validation layers on in release mode, because release mode can have different bugs
    // To make sure it's obvious when validation layers are used, we'll put it in the window title.
    // Note that validation layers degrade performance significantly, so should be disabled in performance metrics and absolutely in production builds
    char title[256] = {};
    snprintf(title, sizeof(title), "Close-quarters Adventure Game [%s]", enabled_validation_layers ? "VULKAN VALIDATION LAYERS ENABLED" : "VULKAN VALIDATION LAYERS DISABLED");
    SDL_Window* window = Core_Init((Core_InitInfo){
        title,
        1280, 720
    });

    // NOTE: Currently checking validation in release mode, but on realse you would normally disable it
    Renderer_InitInfo renderer_info = {
        .window = window,
        .enable_validation = enabled_validation_layers,
        .preferred_initial_settings = {  // Will fallback if these aren't possible
            .uncapped_fps = 1,  // NOTE: <- Enable when gathering FPS metrics
            .msaa_sample_count = 4,
            .fov_y = 50.0f
        }
    };
    Renderer_Init(&renderer_info);

    #if 0  // Just an example of settings API usage
    // Set 4xMSAA if available
    if (Renderer_GetSettingsCapabilities().max_msaa_samples >= 4)
    {
        SDL_Log("Enabling 4xMSAA");
        Renderer_Settings settings = Renderer_GetSettings();
        settings.msaa_sample_count = 4;
        Renderer_ChangeSettings(settings);
    }
    #endif
    GameAudio* game_audio = GameAudio_Init();

    Input_Init("assets/keybindings.json");
    GameUI_Init();
    DebugUI_SetImGuiCallback([](void*){ GameUI_BuildImGui(); }, nullptr);

    // Dunno whether this resource manager will end up in the final build, if no one is integrating it due to more important tasks
    // ResourceManager resource_manager = ResourceManager_Create((ResourceManagerCreateInfo){
    //     .debug_name = "GameAssets",
    //     .initial_capacity = 32
    // });

    // ResourceHandle boot_vert = ResourceManager_RequestBinary(
    //     &resource_manager,
//     RESOURCE_TYPE_SHADER_BYTECODE,
    //     RESOURCE_RESIDENCY_BOOT,
    //     "shader.unlit.vert",
    //     "shaderspv/unlit.vert.spv"
    // );

    // ResourceHandle boot_frag = ResourceManager_RequestBinary(
    //     &resource_manager,
    //     RESOURCE_TYPE_SHADER_BYTECODE,
    //     RESOURCE_RESIDENCY_BOOT,
    //     "shader.unlit.frag",
    //     "shaderspv/unlit.frag.spv"
    // );

    // (void)boot_vert;
    // (void)boot_frag;
    
    /* LOADING NOTES
       Realistically, the splash screen assets would load first, then the main menu assets
       And while the user is on the main menu, we are loading the prefabs.
       That way, we can hide ALL of the latency and it will seem like there are no loading screens at all.
    */

    // Testing Scene and ECS
    Scene scene{};
    scene.StartUp();

    // Asset* room_prefab = scene.LoadPrefab("assets/testing_gen/1O0DOut.gltf");
            //Asset* room_prefab = scene.LoadPrefab("assets/levels/testroom-liamrandomtest-Untitled.gltf");
    //  Asset* many_prefab = scene.LoadPrefab("assets/levels/manylights.gltf");
    Asset* room_prefab = scene.LoadPrefab("assets/levels/testroom_new.gltf");
    // Asset* playground_prefab = scene.LoadPrefab("assets/levels/playground.gltf");
                //Asset* cube_prefab = scene.LoadPrefab("assets/props/simple_cube.gltf");
     Asset* sphere_prefab = scene.LoadPrefab("assets/props/simple_sphere.gltf");
     Asset* small_sphere_prefab = scene.LoadPrefab("assets/props/small_sphere.gltf");
                //Asset* capsule_prefab = scene.LoadPrefab("assets/props/zombie.gltf");
                //Asset* capsule_prefab = scene.LoadPrefab("assets/props/character_capsule.gltf");
    std::vector<Asset*> zombies;
    Asset* zombie = scene.LoadPrefab("assets/animations/zombie.gltf");
    zombies.push_back(zombie);
    Asset* zombie_woman = scene.LoadPrefab("assets/animations/zombie_woman.gltf");
    zombies.push_back(zombie_woman);
    Asset* player = scene.LoadPrefab("assets/animations/player.gltf");
            // TODO: Change the following 2 prefabs so they can be imported (add the boolean "Is ECS Entity" with the new script where it is needed)
            // Asset* catPrefab = scene.LoadPrefab("assets/animations/zomboUntitled.gltf");
            // Asset* catPrefab = scene.LoadPrefab("assets/animations/flatzombo.gltf");

     scene.InstantiatePrefab(room_prefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(capsule_prefab, glm::vec3(0, 5, 2), glm::identity<glm::quat>());
     scene.InstantiatePrefab(sphere_prefab, glm::vec3(4.7, 7, 0.1), glm::identity<glm::quat>());
     scene.InstantiatePrefab(sphere_prefab, glm::vec3(-4.7, 7, -0.1), glm::identity<glm::quat>());
     scene.InstantiatePrefab(sphere_prefab, glm::vec3(0.1, 7, -4.7), glm::identity<glm::quat>());
     scene.InstantiatePrefab(sphere_prefab, glm::vec3(-0.1, 7, 4.7), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(animationPrefab, glm::vec3(5, 20, 0), glm::identity<glm::quat>());
    // // render a second cat
    // EntityID playerEntity = scene.InstantiatePrefab(catPrefab, glm::vec3(10, 0, 10), glm::identity<glm::quat>());


    LevelGeneration generator;
    //LevelFloor floor1 = generator.CreateFullLevel(&scene, "assets/Final_Levels/");
    int levelsSpawned = 0, wave = 1;
    bool zombiesWereSpawned = false;
    //generator.InstantiateLevel(&scene, floor1, zombies, levelsSpawned, wave, {}, {});



    //Asset* animationPrefab = scene.LoadPrefab("assets/animations/cat.gltf");
    
    //scene.InstantiatePrefab(many_prefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    //.InstantiatePrefab(room_prefab, glm::vec3(0.0f, 0.0f, 3.0f), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(playground_prefab, glm::vec3(0, 0, -10.0f), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(room_prefab, glm::vec3(0.0f, 0.0f, 0.0f), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(playground_prefab, glm::vec3(0, 0, 0.0f), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(playground_prefab, glm::vec3(0, 0, -5.0f), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0));
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0));
    /*for(int y = 0; y < 3; y++)
    for (int x = -5; x < 4; x++)
    {
        for (int z = -5; z < 4; z++)
        {
            scene.InstantiatePrefab(sphere_prefab, glm::vec3(x+0.5f, y+3.0f, z+0.5f));
            y += 0.5f;
        }
    }*/
    EntityID playerID = scene.InstantiatePrefab(player, glm::vec3(30, 0, -28), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(zombie_woman, glm::vec3(3, -10.0f, -11.5f), Math::ViewDirToQuat({0.0f ,0.0f, 1.0f}));
    //scene.InstantiatePrefab(zombie_woman, glm::vec3(3, 0.0f, -7.5f), Math::ViewDirToQuat({ 0.0f ,0.0f, 1.0f }));
    //scene.InstantiatePrefab(zombie_woman, glm::vec3(-3, 0.0f, -11.5f), Math::ViewDirToQuat({ 0.0f ,0.0f, 1.0f }));
    //scene.InstantiatePrefab(zombie_woman, glm::vec3(-3, 0.0f, -3.5f), Math::ViewDirToQuat({ 0.0f ,0.0f, 1.0f }));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(4.7, 7, 0.1));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-4.7, 7, -0.1));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(0.1, 7, -4.7));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-0.1, 7, 4.7));
    // scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0));
    // scene.InstantiatePrefab(animationPrefab, glm::vec3(0, 0, 0));
    // render a second cat
    // EntityID playerEntity = scene.InstantiatePrefab(animationPrefab, glm::vec3(10, 0, 10));

    //making gun
    Asset* gun_prefab = scene.LoadPrefab("assets/props/colt.gltf"); 
    EntityID gunID = scene.InstantiatePrefab(gun_prefab, glm::vec3(0, 0, 0));
    // making gun

    
    { // IMPORTANT BIT tying gun to player
        auto& playerSocket = scene.GetECS().GetComponent<C_WeaponSocket>(playerID);
        playerSocket.weapon_entity = gunID;
    }

    scene.SetPlayer(playerID);
    scene.BuildRendererScene();

    MeshPrefab z1_mesh, z2_mesh;
    scene.GetECS().GetView<C_AnimatedMesh>().ForEach([&](C_AnimatedMesh& mesh) {
        // If this entity uses the zombie asset and has valid GPU data
        if (mesh.asset == zombie_woman) {
            z2_mesh = mesh.renderer_prefab;
        }
        else if (mesh.asset == zombie)
        {
            z1_mesh = mesh.renderer_prefab;
        }
    });

    GameUI_SetLevelStartSkillApplyCallback(&scene, GameUI_ApplyPlaceholderLevelStartSkill);
    
    DebugUI_SetECS(&scene.GetECS());
    DebugUI_SetAsset(&scene.m_prefabs);
    InGameCam_Init(&scene.GetECS(), &scene.GetPhysicsManager(), playerID);

    bool running = true;

    // Set up the time tracker for UI
    uint64_t last_time = SDL_GetTicksNS();
    GameState last_ui_state = GameUI_GetState();
    int placeholder_level_index = 0;

    while (running)
    {
		// delta time calculation for testing
        uint64_t current_time = SDL_GetTicksNS();
        float dt = (float)((double)(current_time - last_time) / 1000000000.0);
        last_time = current_time;
        if (dt > 0.1f) dt = 0.1f;

        static double frame_time_accumulation = 0.0;
        static int num_frames_accumulated = 0;
        frame_time_accumulation += (double)dt;
        ++num_frames_accumulated;
        if (num_frames_accumulated >= 30)
        {
            double fps = (double)num_frames_accumulated / frame_time_accumulation;
            
            // Append average FPS to window title
            char new_window_title[256] = {};
            snprintf(new_window_title, sizeof(new_window_title), "%s | FPS=%f", title, fps);
            SDL_SetWindowTitle(window, new_window_title);

            frame_time_accumulation = 0.0;
            num_frames_accumulated = 0;
        }

        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            static b32 window_is_fullscreen = 0;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F11)
            {
                window_is_fullscreen = !window_is_fullscreen;
                SDL_SetWindowFullscreen(window, window_is_fullscreen);
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F1)
            {
                gamestate.disable_hud = !gamestate.disable_hud;
            }

            Renderer_ListenToWindowEvent(event);
            Input_ProcessEvent(event);
        }

        Input_Update();

        if (GameUI_GetState() == GameState::Playing && Input_IsKeyJustPressed(SDL_SCANCODE_P)) //press p to test getting hit
            GameUI_DebugDamagePlayer(scene, 4);

        GameUI_Update();
        if (restart_program)
        {
            break;
        }


        // JANK ASS WAVE SPAWNING/TRACKING //////////////////////////////////////////////////////////////////////////// FIX THE NEXT WAVE BEING INVISIBLE!!!
        int aliveZombies = 0;

        scene.GetECS().GetView<C_Faction, C_Health>().ForEach([&](EntityID e, C_Faction& faction, C_Health& health) {
            if (faction.type == FactionType::Zombie) {
                // Only count them if they actually have health left
                if (health.currentHealth > 0) {
                    aliveZombies++;
                }
            }
            });

        // Dont mess up before starting
        if (aliveZombies > 0) {
            zombiesWereSpawned = true;
        }

        // If all killed, put a message and go next wave
        if (zombiesWereSpawned && aliveZombies == 1) {
            SDL_Log("WAVE %d CLEAR: All zombies were eliminated...", wave);
            wave++;
            //generator.InstantiateLevel(&scene, floor1, zombies, levelsSpawned, 1, z1_mesh, z2_mesh);

        }



        GameState current_ui_state = GameUI_GetState();
        bool current_skill_choice_open = GameUI_IsLevelStartSkillSelectionOpen();
        if (last_ui_state == GameState::MainMenu && current_ui_state == GameState::Playing)
        {
            placeholder_level_index = 1;
            GameUI_OpenLevelStartSkillSelection(placeholder_level_index);
            current_skill_choice_open = true;
        }
        last_ui_state = current_ui_state;

        const bool gameplay_input_enabled = (current_ui_state == GameState::Playing && !current_skill_choice_open);

        // tpcam shoulder toggle
        if (gameplay_input_enabled && Input_IsActionJustPressed(ACTION_TOGGLE_CAMERA)){InGameCam_ToggleShoulder();}
        // pass debug ui edits
        DebugUICameraEdits ui_camera_edits = {};
        if (DebugUI_ConsumeCameraEdits(&ui_camera_edits)){InGameCam_ApplyDebugEdits(ui_camera_edits);}
        // raycast attack at center of screen
        const bool attack_pressed = gameplay_input_enabled && Input_IsActionJustPressed(ACTION_ATTACK);
        const bool aim_held = gameplay_input_enabled && Input_IsActionPressed(ACTION_AIM);
        if (attack_pressed && aim_held)
        {
            const CameraInfo& cam = InGameCam_GetGameplayCamera();

            Ray ray = {};
            ray.origin = cam.position;
            ray.direction = glm::normalize(glm::vec3(
                -cam.view[0][2],
                -cam.view[1][2],
                -cam.view[2][2]
            )); // Forward vector of the camera
            ray.maxDistance = 100.0f; 

            QueryFilterExternal filter = {};
            filter.bodyToIgnore = playerID; 
            filter.hasLayerOfQuery = true;
            filter.layerOfQuery = (uint8_t)BodyLayer::WEAPON; 

            EntityRaycastHit hit = scene.GetPhysicsManager().raycast(ray, filter);
            if (hit.isValid())
            {
                // TODO: Apply damage to hit entity, spawn hit effects, etc.
                SDL_Log("Raycast hit entity %u at point (%f, %f, %f)", hit.entity, hit.point.x, hit.point.y, hit.point.z);
            }
        }
        

        // Only capture mouse while playing (release it on pause), keep relative mouse when debug UI toggled.
        const bool right_mouse_down = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
        SDL_SetWindowRelativeMouseMode(window, (gameplay_input_enabled && !DebugUI_IsOpen()) || (DebugUI_IsOpen() && right_mouse_down));

        // controller test
        scene.GetECS().GetView<C_PlayerInput>().ForEach([&](C_PlayerInput& input) {

            input.move_forward = gameplay_input_enabled && Input_IsActionPressed(ACTION_MOVE_FORWARD);
            input.forward = gameplay_input_enabled ? Input_GetActionValue(ACTION_MOVE_FORWARD) : 0.0f;

            input.move_backward = gameplay_input_enabled && Input_IsActionPressed(ACTION_MOVE_BACKWARD);
            input.backward = gameplay_input_enabled ? Input_GetActionValue(ACTION_MOVE_BACKWARD) : 0.0f;

            input.move_left = gameplay_input_enabled && Input_IsActionPressed(ACTION_MOVE_LEFT);
            input.left = gameplay_input_enabled ? Input_GetActionValue(ACTION_MOVE_LEFT) : 0.0f;

            input.move_right = gameplay_input_enabled && Input_IsActionPressed(ACTION_MOVE_RIGHT);
            input.right = gameplay_input_enabled ? Input_GetActionValue(ACTION_MOVE_RIGHT) : 0.0f;

            input.jump = gameplay_input_enabled && Input_IsActionJustPressed(ACTION_JUMP);
            input.crouch = gameplay_input_enabled && Input_IsActionPressed(ACTION_CROUCH);
            input.run = gameplay_input_enabled && Input_IsActionPressed(ACTION_SPRINT);
            input.aim = gameplay_input_enabled && Input_IsActionPressed(ACTION_AIM);
            input.attack = gameplay_input_enabled && Input_IsActionJustPressed(ACTION_ATTACK);
            }
        );
        // Movement always follows camera forward.
        scene.SetMovementCameraForward(InGameCam_GetMovementForward());

        // Game ticks
        if (current_ui_state == GameState::Playing && !GameUI_IsLevelStartSkillSelectionOpen()) // freeze game when not playing
        {
            
            scene.Update(dt);

            // Update in-game camera
            InGameCam_Update(dt, gameplay_input_enabled, DebugUI_IsOpen(), right_mouse_down, DebugUI_GetCameraMode());

            // pass camera snapshot to debug UI
            const InGameCamSnapshot ingame_cam_snapshot = InGameCam_GetSnapshot();
            DebugUI_SetInGameCameraSnapshot(&ingame_cam_snapshot);

            GameUI_UpdatePlayingHUD(scene);
        }
        GameAudio_Update(game_audio, scene, InGameCam_GetGameplayCamera(), dt);

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            scene.Render();

            Renderer_DrawFrame(DebugUI_IsOpen() ? DebugUI_GetCameraInfo(dt) : InGameCam_GetGameplayCamera());
        }
        // Quit if requested from any menu
        if (GameUI_GetState() == GameState::Quitting) running = false;
    }

    InGameCam_Shutdown();
    scene.Shutdown();
    GameAudio_Destroy(game_audio);
    Input_Shutdown();
    Renderer_Shutdown();
    Core_Shutdown(window);

    if (restart_program)
    {
        restart_program = false;
        main(0, NULL);
    }


    return 0;
}
