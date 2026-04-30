#include "core/core.h"
#include "core/input.h"
#include "core/input.h"
#include "renderer/renderer.h"
#include "renderer/debug_ui_api.h"
#include "foundations/scene.h"
#include "core/components.h"
#include "core/animation.h"
#include "game_ui.h"
#include "fp_cam.h"
#include "tp_cam.h"
#include "game/foundations/components.h"
#include "core/audio_system.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

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
            .uncapped_fps = 0,
            .msaa_sample_count = 1,
            .fov_y = 50.0f
        }
    };
    Renderer_Init(&renderer_info);

    AudioSystem audio_system = AudioSystem_Create((AudioSystemCreateInfo){
        .debug_name = "GameAudio",
        .initial_capacity = 8,
        .master_volume = 1.0f
    });
    AudioSystem_LogSummary(&audio_system);

    AudioClipHandle startup_music = AudioSystem_LoadClipEx(
        &audio_system,
        "startup_music",
        "All_Sounds_MP3_UNMASTERED2/Low_Winds.mp3",
        AUDIO_CLIP_CATEGORY_SOUNDTRACK
    );
    AudioClipHandle startup_test_sfx = AudioSystem_LoadClipEx(
        &audio_system,
        "startup_test_sfx",
        "All_Sounds_MP3_UNMASTERED2/TV_Static.mp3",
        AUDIO_CLIP_CATEGORY_SFX
    );

    if (startup_music != 0)
    {
        if (!AudioSystem_PlaySoundtrackLoop(&audio_system, startup_music, 0.80f))
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: soundtrack loaded but failed to start playback.");
        }
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to load startup soundtrack.");
    }

    if (startup_test_sfx != 0)
    {
        if (!AudioSystem_PlaySFXOneShot(&audio_system, startup_test_sfx, 1.0f))
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: startup test SFX loaded but failed to start playback.");
        }
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to load startup test SFX.");
    }

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

    // Set 4xMSAA to test settings API
    if (Renderer_GetSettingsCapabilities().max_msaa_samples >= 4)
    {
        Renderer_Settings settings = Renderer_GetSettings();
        settings.msaa_sample_count = 4;
        Renderer_ChangeSettings(settings);
    }

    
    /* LOADING NOTES
       Realistically, the splash screen assets would load first, then the main menu assets
       And while the user is on the main menu, we are loading the prefabs.
       That way, we can hide ALL of the latency and it will seem like there are no loading screens at all.
    */

    // Testing Scene and ECS
    Scene scene{};
    scene.StartUp();

    Asset* room_prefab = scene.LoadPrefab("assets/levels/lightroom1.gltf");
    // Asset* cube_prefab = scene.LoadPrefab("assets/props/simple_cube.gltf");
    // Asset* sphere_prefab = scene.LoadPrefab("assets/props/simple_sphere.gltf");
    // Asset* capsule_prefab = scene.LoadPrefab("assets/props/simple_capsule.gltf");
    // TODO: Change the following 2 prefabs so they can be imported (add the boolean "Is ECS Entity" with the new script where it is needed)
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/scene.gltf");
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/flatzombo.gltf");
    Asset* animationPrefab = scene.LoadPrefab("assets/animations/cat.gltf");

    scene.InstantiatePrefab(room_prefab, glm::vec3(0, 0, 0));
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0));
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0));
    // scene.InstantiatePrefab(capsule_prefab, glm::vec3(0, 5, 2));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(4.7, 7, 0.1));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-4.7, 7, -0.1));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(0.1, 7, -4.7));
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-0.1, 7, 4.7));
    // scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0));
    // scene.InstantiatePrefab(animationPrefab, glm::vec3(0, 0, 0));
    // render a second cat
    // EntityID playerEntity = scene.InstantiatePrefab(animationPrefab, glm::vec3(10, 0, 10));

    scene.BuildRendererScene();

    // TODO: Debug UI is built around the idea of 1 asset at the moment.
    //       This must change with the new scene system that can load many asset prefabs.
    DebugUI_SetECS(&scene.GetECS());
    DebugUI_SetAsset(room_prefab);

    // Game owns FP/TP camera state; seed both from Debug UI state once at startup.
    FPCamState game_fp_cam = {};
    if (const FPCamState* initial_fp_cam = DebugUI_GetFPCamState())
        game_fp_cam = *initial_fp_cam;

    TPCamState game_tp_cam = {};
    if (const TPCamState* initial_tp_cam = DebugUI_GetTPCamState())
        game_tp_cam = *initial_tp_cam;

    // Publish initial camera snapshots so debug camera switching is valid on frame 0.
    CameraInfo initial_fp_camera = Game::FPCam_Update(game_fp_cam, &scene.GetECS(), 0.0f, false, false);
    CameraInfo initial_tp_camera = Game::TPCam_Update(game_tp_cam, &scene.GetECS(), 0.0f, false, true);
    DebugUI_SetFPCamState(&game_fp_cam);
    DebugUI_SetTPCamState(&game_tp_cam);
    DebugUI_SetFPCamCameraInfo(&initial_fp_camera);
    DebugUI_SetTPCamCameraInfo(&initial_tp_camera);

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
        bool toggle_fp_tp_camera = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_V)
                toggle_fp_tp_camera = true;

            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
                toggle_fp_tp_camera = true;

            Renderer_ListenToWindowEvent(event);
            Input_ProcessEvent(event);
        }
        Input_Update();
        GameUI_Update();

        if (toggle_fp_tp_camera)
        {
            DebugUICameraMode mode = DebugUI_GetGameplayCameraMode();
            mode = (mode == DebugUICameraMode::TPCam) ? DebugUICameraMode::FPCam : DebugUICameraMode::TPCam;
            DebugUI_SetGameplayCameraMode(mode);
        }

        // Quit if requested from any menu
        if (GameUI_GetState() == GameState::Quitting) running = false;

        // Only capture mouse while playing (release it on menus)
        bool is_playing = GameUI_GetState() == GameState::Playing;
        bool debug_ui_open = DebugUI_IsOpen();
        Uint32 mouse_buttons = SDL_GetMouseState(nullptr, nullptr);
        bool right_mouse_down = (mouse_buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
        // Keep relative mouse while actively controlling camera (gameplay or Debug UI RMB drag).
        bool use_relative_mouse = (is_playing && !debug_ui_open) || (debug_ui_open && right_mouse_down);
        SDL_SetWindowRelativeMouseMode(window, use_relative_mouse);

        // controller test not ideal at all
        const bool* state = SDL_GetKeyboardState(NULL);

        scene.GetECS().GetView<C_AnimatedMesh, C_PlayerInput>().ForEach([&](EntityID e, C_AnimatedMesh&, C_PlayerInput& input) {
                input.move_forward = state[SDL_SCANCODE_K];
                input.move_backward = state[SDL_SCANCODE_I];
                input.move_left = state[SDL_SCANCODE_L];
                input.move_right = state[SDL_SCANCODE_J];
                input.jump = state[SDL_SCANCODE_SPACE];
            }
        );

        // Game ticks
        scene.Update(dt);
        AudioSystem_Update(&audio_system, dt);

        // Pull latest edits from Debug UI (bind target/FOV/mode-facing state).
        if (const FPCamState* debug_fp_cam = DebugUI_GetFPCamState())
            game_fp_cam = *debug_fp_cam;

        if (const TPCamState* debug_tp_cam = DebugUI_GetTPCamState())
            game_tp_cam = *debug_tp_cam;

        DebugUICameraMode gameplay_camera_mode = DebugUI_GetGameplayCameraMode();
        DebugUICameraMode debug_camera_mode = DebugUI_GetCameraMode();
        DebugUICameraMode active_fp_tp_mode = debug_ui_open ? debug_camera_mode : gameplay_camera_mode;

        // In debug mode, RMB drag still drives look even outside normal playing state.
        bool allow_mouse_look = (is_playing && !debug_ui_open) || (debug_ui_open && right_mouse_down);

        // The active FP/TP mode depends on context: debug camera mode when UI is open, gameplay mode otherwise.
        bool fp_active = active_fp_tp_mode == DebugUICameraMode::FPCam;
        bool tp_active = active_fp_tp_mode == DebugUICameraMode::TPCam;

        bool fp_allow_mouse_look = allow_mouse_look && fp_active;
        bool tp_allow_mouse_look = allow_mouse_look && tp_active;

        // Freeze non-input simulation drift while paused/menu.
        float fp_cam_dt = (is_playing && fp_active) ? dt : 0.0f;
        float tp_cam_dt = (is_playing && tp_active) ? dt : 0.0f;
        bool fp_apply_fov = fp_active;
        bool tp_apply_fov = tp_active;

        CameraInfo game_fp_camera = Game::FPCam_Update(game_fp_cam, &scene.GetECS(), fp_cam_dt, fp_allow_mouse_look, fp_apply_fov);
        CameraInfo game_tp_camera = Game::TPCam_Update(game_tp_cam, &scene.GetECS(), tp_cam_dt, tp_allow_mouse_look, tp_apply_fov);

        // Push resolved state/camera back to Debug UI so panels display authoritative game data.
        DebugUI_SetFPCamState(&game_fp_cam);
        DebugUI_SetTPCamState(&game_tp_cam);
        DebugUI_SetFPCamCameraInfo(&game_fp_camera);
        DebugUI_SetTPCamCameraInfo(&game_tp_camera);

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            scene.Render();

            Renderer_DrawFrame(DebugUI_GetCameraInfo(dt));
        }
    }

    scene.Shutdown();
    //AudioSystem_Destroy(&audio_system);
    Input_Shutdown();
    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
