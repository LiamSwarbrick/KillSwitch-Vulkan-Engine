#include "core/core.h"
#include "core/input.h"
#include "renderer/renderer.h"
#include "renderer/debug_ui_api.h"
#include "foundations/scene.h"
#include "core/components.h"
#include "core/animation.h"
#include "game_ui.h"
#include "ingame_cam.h"
#include "game/foundations/components.h"
#include "core/audio_system.h"
#include "physics/body_layers.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

static void ApplyPlaceholderLevelStartSkill(Scene& scene, const LevelStartSkillSelection& selection)
{
    ECS& ecs = scene.GetECS();
    (void)ecs;

    SDL_Log(
        "GameUI: selected level-start skill [level=%d, slot=%d, id=%s, name=%s]",
        selection.level_index,
        selection.selected_index,
        selection.selected_option.skill_id ? selection.selected_option.skill_id : "<null>",
        selection.selected_option.display_name ? selection.selected_option.display_name : "<null>");

    // TODO: Apply real gameplay modifications here.
    // Suggested future integrations:
    // - mutate gameplay components through scene.GetECS()
    // - drive presentation through PlayAnim / PlayUpperBodyAnim on animated entities
    // - store persistent run modifiers in scene-owned gameplay state
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bool enabled_validation_layers = true;
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
            .uncapped_fps = 0,  // NOTE: <- Enable when gathering FPS metrics
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


    AudioSystem audio_system = AudioSystem_Create((AudioSystemCreateInfo){
        .debug_name = "GameAudio",
        .initial_capacity = 8,
        .master_volume = 1.0f
    });
    AudioSystem_LogSummary(&audio_system);

    AudioClipHandle startup_music = AudioSystem_LoadClip(
        &audio_system,
        "startup_music",
        "All_Sounds_MP3_UNMASTERED2/Bad_Signs.mp3"    
    );
    // Set up spatial audio for player start position
    // if (startup_music != 0)
    //     {
    //         AudioSystem_SetClipMinMaxDistance(&audio_system, startup_music, 1.0f, 40.0f);
    //         AudioSystem_PlaySpatialLoop(
    //             &audio_system,
    //             startup_music,
    //             1.0f,
    //             2.0f, 1.0f, 0.0f
    //         );
    //     }

    // AudioClipHandle startup_test_sfx = AudioSystem_LoadClipEx(
    //     &audio_system,
    //     "startup_test_sfx",
    //     "All_Sounds_MP3_UNMASTERED2/TV_Static.mp3",
    //     AUDIO_CLIP_CATEGORY_SFX
    // );

    // if (startup_music != 0)
    // {
    //     if (!AudioSystem_PlaySoundtrackLoop(&audio_system, startup_music, 0.1f))
    //     {
    //         SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: soundtrack loaded but failed to start playback.");
    //     }
    // }
    // else
    // {
    //     SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to load startup soundtrack.");
    // }

    // if (startup_test_sfx != 0)
    // {
    //     if (!AudioSystem_PlaySFXOneShot(&audio_system, startup_test_sfx, 0.1f))
    //     {
    //         SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: startup test SFX loaded but failed to start playback.");
    //     }
    // }
    // else
    // {
    //     SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to load startup test SFX.");
    // }

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

    Asset* room_prefab = scene.LoadPrefab("assets/levels/testroom_new.gltf");
    Asset* many_prefab = scene.LoadPrefab("assets/levels/manylights.gltf");
    Asset* playground_prefab = scene.LoadPrefab("assets/levels/playground.gltf");
    Asset* cube_prefab = scene.LoadPrefab("assets/props/simple_cube.gltf");
    Asset* sphere_prefab = scene.LoadPrefab("assets/props/simple_sphere.gltf");
    //Asset* capsule_prefab = scene.LoadPrefab("assets/props/zombie.gltf");
    Asset* capsule_prefab = scene.LoadPrefab("assets/props/character_capsule.gltf");
    Asset* zombie_woman = scene.LoadPrefab("assets/animations/zombie_woman.gltf");
    Asset* player = scene.LoadPrefab("assets/animations/player.gltf");
    // TODO: Change the following 2 prefabs so they can be imported (add the boolean "Is ECS Entity" with the new script where it is needed)
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/zomboUntitled.gltf");
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/flatzombo.gltf");
    //Asset* animationPrefab = scene.LoadPrefab("assets/animations/cat.gltf");
        
    scene.InstantiatePrefab(many_prefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    scene.InstantiatePrefab(room_prefab, glm::vec3(0.0f, 0.0f, -10.0f), glm::identity<glm::quat>());
    scene.InstantiatePrefab(playground_prefab, glm::vec3(0, 0, -10.0f), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0));
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0));ss
    
    EntityID playerID = scene.InstantiatePrefab(player, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    scene.InstantiatePrefab(zombie_woman, glm::vec3(0, 0, 2.0f), glm::identity<glm::quat>());
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

    scene.SetPlayer(playerID);
    scene.BuildRendererScene();
    GameUI_SetLevelStartSkillApplyCallback(&scene, ApplyPlaceholderLevelStartSkill);

    // TODO: Debug UI is built around the idea of 1 asset at the moment.
    //       This must change with the new scene system that can load many asset prefabs.
    DebugUI_SetECS(&scene.GetECS());
    DebugUI_SetAsset(&scene.m_prefabs);
    InGameCam_Init(&scene.GetECS(), &scene.GetPhysicsManager(), playerID);

    bool running = true;

    // Set up the time tracker
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

            Renderer_ListenToWindowEvent(event);
            Input_ProcessEvent(event);
        }

        Input_Update();
        GameUI_Update();

        GameState current_ui_state = GameUI_GetState();
        if (last_ui_state == GameState::MainMenu && current_ui_state == GameState::Playing)
        {
            placeholder_level_index = 1;
            GameUI_OpenLevelStartSkillSelection(placeholder_level_index);
        }
        last_ui_state = current_ui_state;

        const bool gameplay_input_enabled = (current_ui_state == GameState::Playing && !GameUI_IsLevelStartSkillSelectionOpen());

        // tpcam shoulder toggle
        if (gameplay_input_enabled && Input_IsActionJustPressed(ACTION_TOGGLE_CAMERA)){InGameCam_ToggleShoulder();}
        // pass debug ui edits
        DebugUICameraEdits ui_camera_edits = {};
        if (DebugUI_ConsumeCameraEdits(&ui_camera_edits)){InGameCam_ApplyDebugEdits(ui_camera_edits);}
        // raycast attack at center of screen
        if (gameplay_input_enabled &&
            Input_IsActionJustPressed(ACTION_ATTACK) && Input_IsActionPressed(ACTION_AIM))
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
            }
        );
        // Movement always follows camera forward.
        scene.SetMovementCameraForward(InGameCam_GetMovementForward());

        // Game ticks
        scene.Update(dt);

        // Update in-game camera
    InGameCam_Update(dt, gameplay_input_enabled, DebugUI_IsOpen(), right_mouse_down, DebugUI_GetCameraMode());
        // pass camera snapshot to debug UI
        const InGameCamSnapshot ingame_cam_snapshot = InGameCam_GetSnapshot();
        DebugUI_SetInGameCameraSnapshot(&ingame_cam_snapshot);

        // update spatial audio with audio position
        glm::vec3 listener_pos = InGameCam_GetGameplayCamera().position;
        glm::vec3 listener_forward = glm::normalize(glm::vec3(
            -InGameCam_GetGameplayCamera().view[0][2],
            -InGameCam_GetGameplayCamera().view[1][2],
            -InGameCam_GetGameplayCamera().view[2][2]
        ));
        glm::vec3 listener_up = glm::normalize(glm::vec3(
            InGameCam_GetGameplayCamera().view[0][1],
            InGameCam_GetGameplayCamera().view[1][1],
            InGameCam_GetGameplayCamera().view[2][1]
        ));

        AudioSystem_UpdateSpatialState(
            &audio_system, 
            listener_pos.x, listener_pos.y, listener_pos.z, 
            listener_forward.x, listener_forward.y, listener_forward.z,
            listener_up.x, listener_up.y, listener_up.z,
            nullptr, 1
        );

        AudioSystem_Update(&audio_system, dt);

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
    AudioSystem_Destroy(&audio_system);
    Input_Shutdown();
    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
