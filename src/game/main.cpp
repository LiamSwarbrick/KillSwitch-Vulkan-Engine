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

#include "core/utils/math_utils.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "foundations/level/LevelGeneration.h"

struct GameplayAudioClips
{
    static constexpr int FootstepVariantCount = 3;
    static constexpr int CrouchFootstepVariantCount = 2;
    static constexpr int ZombieGroanVariantCount = 2;
    static constexpr int ZombieStepVariantCount = 2;

    AudioClipHandle soundtrack_loop = 0;
    AudioClipHandle ambient_loop = 0;
    AudioClipHandle weapon_fire = 0;
    AudioClipHandle weapon_dry_fire = 0;
    AudioClipHandle bullet_impact = 0;
    AudioClipHandle weapon_hit_flesh = 0;
    AudioClipHandle jump_takeoff = 0;
    AudioClipHandle land_soft = 0;
    AudioClipHandle land_hard = 0;
    AudioClipHandle footstep_walk[FootstepVariantCount] = {};
    AudioClipHandle footstep_sprint[FootstepVariantCount] = {};
    AudioClipHandle footstep_crouch[CrouchFootstepVariantCount] = {};
    AudioClipHandle zombie_alert = 0;
    AudioClipHandle zombie_groan[ZombieGroanVariantCount] = {};
    AudioClipHandle zombie_attack = 0;
    AudioClipHandle zombie_step[ZombieStepVariantCount] = {};
    AudioClipHandle ui_select = 0;
    AudioClipHandle ui_back = 0;
    AudioClipHandle ui_pause = 0;
};

static AudioClipHandle LoadGameplayClip(
    AudioSystem* audio_system,
    const char* logical_name,
    const char* path,
    const char* fallback_path,
    AudioClipCategory category)
{
    AudioClipHandle handle = AudioSystem_LoadClipEx(audio_system, logical_name, path, category);
    if (handle == 0 && fallback_path != nullptr && fallback_path[0] != '\0')
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: primary path missing for '%s'; trying '%s'.", logical_name, fallback_path);
        handle = AudioSystem_LoadClipEx(audio_system, logical_name, fallback_path, category);
    }

    if (handle == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load '%s' from '%s'.", logical_name, path);
    }
    return handle;
}

static void LoadGameplayClipVariants(
    AudioSystem* audio_system,
    AudioClipHandle* handles,
    int handle_count,
    const char* logical_prefix,
    const char* const* paths,
    AudioClipCategory category)
{
    for (int index = 0; index < handle_count; ++index)
    {
        char logical_name[AUDIO_NAME_MAX_LEN] = {};
        snprintf(logical_name, sizeof(logical_name), "%s_%02d", logical_prefix, index + 1);
        handles[index] = LoadGameplayClip(audio_system, logical_name, paths[index], nullptr, category);
    }
}

static AudioClipHandle SelectClipVariant(const AudioClipHandle* handles, int handle_count, u32* cursor)
{
    if (handles == nullptr || handle_count <= 0 || cursor == nullptr)
    {
        return 0;
    }

    for (int attempt = 0; attempt < handle_count; ++attempt)
    {
        const u32 index = (*cursor + (u32)attempt) % (u32)handle_count;
        if (handles[index] != 0)
        {
            *cursor = (index + 1u) % (u32)handle_count;
            return handles[index];
        }
    }

    return 0;
}

static GameplayAudioClips LoadGameplayAudio(AudioSystem* audio_system)
{
    GameplayAudioClips clips = {};

    clips.soundtrack_loop = LoadGameplayClip(
        audio_system,
        "gameplay_soundtrack_loop",
        "assets/sounds/gameplay/soundtrack_loop.mp3",
        "assets/sounds/All_Sounds_MP3_UNMASTERED2/Bad_Signs.mp3",
        AUDIO_CLIP_CATEGORY_SOUNDTRACK);
    clips.ambient_loop = LoadGameplayClip(
        audio_system,
        "gameplay_ambient_loop",
        "assets/sounds/gameplay/ambient_loop.mp3",
        "assets/sounds/All_Sounds_MP3_UNMASTERED2/Deep_Forest_Random_Drone.mp3",
        AUDIO_CLIP_CATEGORY_AMBIENT);
    clips.weapon_fire = LoadGameplayClip(
        audio_system,
        "weapon_fire",
        "assets/sounds/gameplay/weapon_fire.mp3",
        "assets/sounds/All_Sounds_MP3_UNMASTERED2/drive_by.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.weapon_dry_fire = LoadGameplayClip(
        audio_system,
        "weapon_dry_fire",
        "assets/sounds/gameplay/weapon_dry_fire.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.bullet_impact = LoadGameplayClip(
        audio_system,
        "bullet_impact",
        "assets/sounds/gameplay/bullet_impact.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.weapon_hit_flesh = LoadGameplayClip(
        audio_system,
        "weapon_hit_flesh",
        "assets/sounds/gameplay/weapon_hit_flesh.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.jump_takeoff = LoadGameplayClip(
        audio_system,
        "jump_takeoff",
        "assets/sounds/gameplay/jump_takeoff.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.land_soft = LoadGameplayClip(
        audio_system,
        "land_soft",
        "assets/sounds/gameplay/land_soft.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.land_hard = LoadGameplayClip(
        audio_system,
        "land_hard",
        "assets/sounds/gameplay/land_hard.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);

    const char* walk_footsteps[] = {
        "assets/sounds/gameplay/footstep_walk_01.wav",
        "assets/sounds/gameplay/footstep_walk_02.wav",
        "assets/sounds/gameplay/footstep_walk_03.wav",
    };
    LoadGameplayClipVariants(
        audio_system,
        clips.footstep_walk,
        GameplayAudioClips::FootstepVariantCount,
        "footstep_walk",
        walk_footsteps,
        AUDIO_CLIP_CATEGORY_SFX);

    const char* sprint_footsteps[] = {
        "assets/sounds/gameplay/footstep_sprint_01.wav",
        "assets/sounds/gameplay/footstep_sprint_02.wav",
        "assets/sounds/gameplay/footstep_sprint_03.wav",
    };
    LoadGameplayClipVariants(
        audio_system,
        clips.footstep_sprint,
        GameplayAudioClips::FootstepVariantCount,
        "footstep_sprint",
        sprint_footsteps,
        AUDIO_CLIP_CATEGORY_SFX);

    const char* crouch_footsteps[] = {
        "assets/sounds/gameplay/footstep_crouch_01.wav",
        "assets/sounds/gameplay/footstep_crouch_02.wav",
    };
    LoadGameplayClipVariants(
        audio_system,
        clips.footstep_crouch,
        GameplayAudioClips::CrouchFootstepVariantCount,
        "footstep_crouch",
        crouch_footsteps,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.zombie_alert = LoadGameplayClip(
        audio_system,
        "zombie_alert",
        "assets/sounds/gameplay/zombie_alert_short.wav",
        "assets/sounds/gameplay/zombie_alert.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.zombie_groan[0] = LoadGameplayClip(
        audio_system,
        "zombie_groan_01",
        "assets/sounds/gameplay/zombie_groan_short_01.wav",
        "assets/sounds/gameplay/zombie_groan.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.zombie_groan[1] = LoadGameplayClip(
        audio_system,
        "zombie_groan_02",
        "assets/sounds/gameplay/zombie_groan_short_02.wav",
        "assets/sounds/gameplay/zombie_groan.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.zombie_attack = LoadGameplayClip(
        audio_system,
        "zombie_attack",
        "assets/sounds/gameplay/zombie_attack_short.wav",
        "assets/sounds/gameplay/zombie_attack.mp3",
        AUDIO_CLIP_CATEGORY_SFX);

    const char* zombie_steps[] = {
        "assets/sounds/gameplay/zombie_step_01.wav",
        "assets/sounds/gameplay/zombie_step_02.wav",
    };
    LoadGameplayClipVariants(
        audio_system,
        clips.zombie_step,
        GameplayAudioClips::ZombieStepVariantCount,
        "zombie_step",
        zombie_steps,
        AUDIO_CLIP_CATEGORY_SFX);

    clips.ui_select = LoadGameplayClip(
        audio_system,
        "ui_select",
        "assets/sounds/gameplay/ui_select.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.ui_back = LoadGameplayClip(
        audio_system,
        "ui_back",
        "assets/sounds/gameplay/ui_back.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);
    clips.ui_pause = LoadGameplayClip(
        audio_system,
        "ui_pause",
        "assets/sounds/gameplay/ui_pause.wav",
        nullptr,
        AUDIO_CLIP_CATEGORY_SFX);

    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SOUNDTRACK, 0.55f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_AMBIENT, 0.65f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SFX, 1.0f);

    if (clips.soundtrack_loop != 0 && !AudioSystem_PlaySoundtrackLoop(audio_system, clips.soundtrack_loop, 0.32f))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: soundtrack loaded but did not start.");
    }

    if (clips.ambient_loop != 0 && !AudioSystem_PlayAmbientLoop(audio_system, clips.ambient_loop, 0.42f))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: ambient loop loaded but did not start.");
    }

    return clips;
}

static void PlayGameplaySFXAt(
    AudioSystem* audio_system,
    AudioClipHandle handle,
    float volume,
    const glm::vec3& position,
    float min_distance,
    float max_distance)
{
    if (handle == 0)
    {
        return;
    }

    AudioSystem_PlayEntitySFXAt(
        audio_system,
        handle,
        volume,
        position.x, position.y, position.z,
        min_distance,
        max_distance);
}

static void PlayGameplaySFX(AudioSystem* audio_system, AudioClipHandle handle, float volume)
{
    if (handle == 0)
    {
        return;
    }

    AudioSystem_SetClipSpatialized(audio_system, handle, 0);
    AudioSystem_PlaySFXOneShot(audio_system, handle, volume);
}

static void ApplyGameplayAudioMix(AudioSystem* audio_system, GameState state, bool skill_choice_open)
{
    float soundtrack_volume = 0.55f;
    float ambient_volume = 0.65f;

    if (state == GameState::MainMenu)
    {
        soundtrack_volume = 0.50f;
        ambient_volume = 0.28f;
    }
    else if (state == GameState::Paused || skill_choice_open)
    {
        soundtrack_volume = 0.42f;
        ambient_volume = 0.35f;
    }

    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SOUNDTRACK, soundtrack_volume);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_AMBIENT, ambient_volume);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SFX, 1.0f);
}

static void PlayUITransitionAudio(
    AudioSystem* audio_system,
    const GameplayAudioClips& clips,
    GameState previous_state,
    GameState current_state)
{
    if (previous_state == current_state)
    {
        return;
    }

    if (current_state == GameState::Paused)
    {
        PlayGameplaySFX(audio_system, clips.ui_pause, 0.58f);
    }
    else if (current_state == GameState::MainMenu || current_state == GameState::Quitting)
    {
        PlayGameplaySFX(audio_system, clips.ui_back, 0.55f);
    }
    else if (current_state == GameState::Playing)
    {
        PlayGameplaySFX(audio_system, clips.ui_select, 0.50f);
    }
}

static void UpdatePlayerMovementAudio(
    Scene& scene,
    AudioSystem* audio_system,
    const GameplayAudioClips& clips,
    float dt,
    bool gameplay_audio_enabled)
{
    static float footstep_timer = 0.08f;
    static bool was_grounded = true;
    static bool was_jumping = false;
    static u32 walk_footstep_index = 0;
    static u32 sprint_footstep_index = 0;
    static u32 crouch_footstep_index = 0;

    if (!gameplay_audio_enabled)
    {
        footstep_timer = 0.08f;
        return;
    }

    bool processed_player = false;
    scene.GetECS().GetView<C_Transform, C_MovementInfo, C_PlayerInput>().ForEach(
        [&](C_Transform& transform, C_MovementInfo& move_info, C_PlayerInput&)
        {
            if (processed_player)
            {
                return;
            }

            processed_player = true;
            const glm::vec3 position = glm::vec3(transform.matrix[3]);

            if (!was_jumping && move_info.isJumping)
            {
                PlayGameplaySFXAt(audio_system, clips.jump_takeoff, 0.42f, position, 0.75f, 18.0f);
            }

            if (!was_grounded && move_info.isGrounded)
            {
                const float horizontal_speed = glm::length(glm::vec3(move_info.velocity.x, 0.0f, move_info.velocity.z));
                const AudioClipHandle land_clip = horizontal_speed > 4.5f ? clips.land_hard : clips.land_soft;
                PlayGameplaySFXAt(audio_system, land_clip, horizontal_speed > 4.5f ? 0.50f : 0.34f, position, 0.9f, 22.0f);
            }

            was_grounded = move_info.isGrounded;
            was_jumping = move_info.isJumping;

            if (!move_info.isGrounded || !move_info.isMoving)
            {
                footstep_timer = 0.08f;
                return;
            }

            footstep_timer -= dt;
            if (footstep_timer > 0.0f)
            {
                return;
            }

            AudioClipHandle footstep = 0;
            float volume = 0.34f;
            float interval = 0.48f;
            if (move_info.state == MoveState::Sprint)
            {
                footstep = SelectClipVariant(
                    clips.footstep_sprint,
                    GameplayAudioClips::FootstepVariantCount,
                    &sprint_footstep_index);
                volume = 0.50f;
                interval = 0.32f;
            }
            else if (move_info.state == MoveState::Crouch)
            {
                footstep = SelectClipVariant(
                    clips.footstep_crouch,
                    GameplayAudioClips::CrouchFootstepVariantCount,
                    &crouch_footstep_index);
                volume = 0.18f;
                interval = 0.68f;
            }
            else
            {
                footstep = SelectClipVariant(
                    clips.footstep_walk,
                    GameplayAudioClips::FootstepVariantCount,
                    &walk_footstep_index);
            }

            PlayGameplaySFXAt(audio_system, footstep, volume, position, 0.75f, 18.0f);
            footstep_timer = interval;
        });

    if (!processed_player)
    {
        footstep_timer = 0.08f;
        was_grounded = true;
        was_jumping = false;
    }
}

static void UpdateZombieAudio(
    Scene& scene,
    AudioSystem* audio_system,
    const GameplayAudioClips& clips,
    float dt,
    bool gameplay_audio_enabled)
{
    static float zombie_groan_timer = 2.0f;
    static u32 zombie_groan_index = 0;
    static u32 zombie_step_index = 0;

    if (!gameplay_audio_enabled)
    {
        return;
    }

    bool played_state_entry_sound = false;
    scene.GetECS().GetView<C_Transform, C_EnemyAIInfo>().ForEach(
        [&](C_Transform& transform, C_EnemyAIInfo& info)
        {
            if (info.currentState == info.lastAudioState)
            {
                return;
            }

            const bool became_threat =
                info.currentState == C_EnemyAIInfo::Alerted ||
                info.currentState == C_EnemyAIInfo::Chase;
            const bool started_attack = info.currentState == C_EnemyAIInfo::Attack;

            if (!played_state_entry_sound && started_attack)
            {
                glm::vec3 position = glm::vec3(transform.matrix[3]);
                PlayGameplaySFXAt(audio_system, clips.zombie_attack, 0.72f, position, 1.0f, 32.0f);
                played_state_entry_sound = true;
            }
            else if (!played_state_entry_sound && clips.zombie_alert != 0 && became_threat)
            {
                glm::vec3 position = glm::vec3(transform.matrix[3]);
                PlayGameplaySFXAt(audio_system, clips.zombie_alert, 0.65f, position, 1.0f, 35.0f);
                played_state_entry_sound = true;
            }

            info.lastAudioState = info.currentState;
        });

    scene.GetECS().GetView<C_Transform, C_EnemyAIInfo, C_MovementInfo>().ForEach(
        [&](C_Transform& transform, C_EnemyAIInfo& info, C_MovementInfo& move_info)
        {
            const bool can_make_steps =
                info.currentState != C_EnemyAIInfo::Dead &&
                info.currentState != C_EnemyAIInfo::Attack &&
                move_info.isGrounded &&
                move_info.isMoving;

            if (!can_make_steps)
            {
                info.audioFootstepTimer = 0.08f;
                return;
            }

            info.audioFootstepTimer -= dt;
            if (info.audioFootstepTimer > 0.0f)
            {
                return;
            }

            const glm::vec3 position = glm::vec3(transform.matrix[3]);
            const AudioClipHandle step = SelectClipVariant(
                clips.zombie_step,
                GameplayAudioClips::ZombieStepVariantCount,
                &zombie_step_index);
            const bool chasing = info.currentState == C_EnemyAIInfo::Chase;
            PlayGameplaySFXAt(audio_system, step, chasing ? 0.38f : 0.26f, position, 1.0f, 30.0f);
            info.audioFootstepTimer = chasing ? 0.45f : 0.62f;
        });

    zombie_groan_timer -= dt;
    if (zombie_groan_timer > 0.0f)
    {
        return;
    }

    bool played_idle_groan = false;
    scene.GetECS().GetView<C_Transform, C_EnemyAIInfo>().ForEach(
        [&](C_Transform& transform, C_EnemyAIInfo& info)
        {
            if (played_idle_groan || info.currentState == C_EnemyAIInfo::Dead)
            {
                return;
            }

            glm::vec3 position = glm::vec3(transform.matrix[3]);
            const AudioClipHandle groan = SelectClipVariant(
                clips.zombie_groan,
                GameplayAudioClips::ZombieGroanVariantCount,
                &zombie_groan_index);
            if (groan == 0)
            {
                return;
            }

            const float volume = (info.currentState == C_EnemyAIInfo::Chase) ? 0.58f : 0.38f;
            PlayGameplaySFXAt(audio_system, groan, volume, position, 1.5f, 40.0f);
            played_idle_groan = true;
        });

    zombie_groan_timer = played_idle_groan ? 6.0f : 2.0f;
}

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


    AudioSystem audio_system = AudioSystem_Create((AudioSystemCreateInfo){
        .debug_name = "GameAudio",
        .initial_capacity = 64,
        .master_volume = 1.0f
    });
    AudioSystem_LogSummary(&audio_system);
    GameplayAudioClips gameplay_audio = LoadGameplayAudio(&audio_system);

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

    //Asset* room_prefab = scene.LoadPrefab("assets/testing_gen/3DInGreen.gltf");
    //Asset* room_prefab = scene.LoadPrefab("assets/levels/testroom-liamrandomtest-Untitled.gltf");
     Asset* many_prefab = scene.LoadPrefab("assets/levels/manylights.gltf");
    Asset* playground_prefab = scene.LoadPrefab("assets/levels/playground.gltf");
    //Asset* cube_prefab = scene.LoadPrefab("assets/props/simple_cube.gltf");
    Asset* sphere_prefab = scene.LoadPrefab("assets/props/simple_sphere.gltf");
    //Asset* capsule_prefab = scene.LoadPrefab("assets/props/zombie.gltf");
    //Asset* capsule_prefab = scene.LoadPrefab("assets/props/character_capsule.gltf");
    Asset* zombie_woman = scene.LoadPrefab("assets/animations/zombie_woman.gltf");
    Asset* zombie = scene.LoadPrefab("assets/animations/zombie.gltf");
    Asset* player = scene.LoadPrefab("assets/animations/player.gltf");
    // TODO: Change the following 2 prefabs so they can be imported (add the boolean "Is ECS Entity" with the new script where it is needed)
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/zomboUntitled.gltf");
    // Asset* catPrefab = scene.LoadPrefab("assets/animations/flatzombo.gltf");

    // scene.InstantiatePrefab(room_prefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(capsule_prefab, glm::vec3(0, 5, 2), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(4.7, 7, 0.1), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-4.7, 7, -0.1), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(0.1, 7, -4.7), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(sphere_prefab, glm::vec3(-0.1, 7, 4.7), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    // scene.InstantiatePrefab(animationPrefab, glm::vec3(5, 20, 0), glm::identity<glm::quat>());
    // // render a second cat
    // EntityID playerEntity = scene.InstantiatePrefab(catPrefab, glm::vec3(10, 0, 10), glm::identity<glm::quat>());


    LevelGeneration generator;
    LevelFloor floor1 = generator.CreateFullLevel(&scene, "assets/testing_gen/");
    generator.InstantiateLevel(&scene, floor1);



    //Asset* animationPrefab = scene.LoadPrefab("assets/animations/cat.gltf");
    
    //scene.InstantiatePrefab(many_prefab, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    //scene.InstantiatePrefab(room_prefab, glm::vec3(0.0f, 0.0f, -10.0f), glm::identity<glm::quat>());
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
    EntityID playerID = scene.InstantiatePrefab(player, glm::vec3(0, 0, 0), glm::identity<glm::quat>());
    scene.InstantiatePrefab(zombie, glm::vec3(3, 0.0f, -11.5f), Math::ViewDirToQuat({0.0f ,0.0f, 1.0f}));
    scene.InstantiatePrefab(zombie_woman, glm::vec3(3, 0.0f, -7.5f), Math::ViewDirToQuat({ 0.0f ,0.0f, 1.0f }));
    scene.InstantiatePrefab(zombie, glm::vec3(-3, 0.0f, -11.5f), Math::ViewDirToQuat({ 0.0f ,0.0f, 1.0f }));
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
    GameUI_SetLevelStartSkillApplyCallback(&scene, GameUI_ApplyPlaceholderLevelStartSkill);

    DebugUI_SetECS(&scene.GetECS());
    DebugUI_SetAsset(&scene.m_prefabs);
    InGameCam_Init(&scene.GetECS(), &scene.GetPhysicsManager(), playerID);

    bool running = true;

    // Set up the time tracker
    uint64_t last_time = SDL_GetTicksNS();
    GameState last_ui_state = GameUI_GetState();
    bool last_skill_choice_open = GameUI_IsLevelStartSkillSelectionOpen();
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

        if (GameUI_GetState() == GameState::Playing && Input_IsKeyJustPressed(SDL_SCANCODE_P)) //press p to test getting hit
            GameUI_DebugDamagePlayer(scene, 4);

        GameUI_Update();

        GameState current_ui_state = GameUI_GetState();
        bool current_skill_choice_open = GameUI_IsLevelStartSkillSelectionOpen();
        PlayUITransitionAudio(&audio_system, gameplay_audio, last_ui_state, current_ui_state);
        if (last_ui_state == GameState::MainMenu && current_ui_state == GameState::Playing)
        {
            placeholder_level_index = 1;
            GameUI_OpenLevelStartSkillSelection(placeholder_level_index);
            current_skill_choice_open = true;
        }

        if (last_skill_choice_open != current_skill_choice_open)
        {
            PlayGameplaySFX(&audio_system, current_skill_choice_open ? gameplay_audio.ui_pause : gameplay_audio.ui_select, 0.42f);
        }

        ApplyGameplayAudioMix(&audio_system, current_ui_state, current_skill_choice_open);
        last_ui_state = current_ui_state;
        last_skill_choice_open = current_skill_choice_open;

        const bool gameplay_input_enabled = (current_ui_state == GameState::Playing && !current_skill_choice_open);
        const bool gameplay_world_audio_enabled = (current_ui_state == GameState::Playing && !current_skill_choice_open);

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
            PlayGameplaySFX(&audio_system, gameplay_audio.weapon_fire, 0.9f);

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

                ECS& ecs = scene.GetECS();
                if (ecs.Has<C_Faction>(hit.entity) && ecs.GetComponent<C_Faction>(hit.entity).type == FactionType::Zombie)
                {
                    PlayGameplaySFXAt(&audio_system, gameplay_audio.weapon_hit_flesh, 0.58f, hit.point, 1.0f, 35.0f);
                }
                else
                {
                    PlayGameplaySFXAt(&audio_system, gameplay_audio.bullet_impact, 0.45f, hit.point, 1.0f, 30.0f);
                }
            }
        }
        else if (attack_pressed)
        {
            PlayGameplaySFX(&audio_system, gameplay_audio.weapon_dry_fire, 0.48f);
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

    UpdatePlayerMovementAudio(scene, &audio_system, gameplay_audio, dt, gameplay_world_audio_enabled);

    UpdateZombieAudio(
        scene,
        &audio_system,
        gameplay_audio,
        dt,
        current_ui_state == GameState::Playing
    );

    // Update in-game camera
    InGameCam_Update(dt, gameplay_input_enabled, DebugUI_IsOpen(), right_mouse_down, DebugUI_GetCameraMode());

    // pass camera snapshot to debug UI
    const InGameCamSnapshot ingame_cam_snapshot = InGameCam_GetSnapshot();
    DebugUI_SetInGameCameraSnapshot(&ingame_cam_snapshot);

    GameUI_UpdatePlayingHUD(scene);
}
        
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
            nullptr, 0
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
