#include "game_audio.h"
#include <unordered_map>
#include "SDL3/SDL.h"
#include "core/audio_system.h"
#include "foundations/components.h"
#include "foundations/scene.h"
#include "game_ui.h"
#include "ingame_cam.h"

struct GameplayAudioClips
{
    AudioClipHandle soundtrack_loop = 0;
    AudioClipHandle weapon_fire = 0;
    AudioClipHandle weapon_dry_fire = 0;
    AudioClipHandle weapon_reload = 0;
    AudioClipHandle footstep = 0;
    AudioClipHandle zombie_alert = 0;
    AudioClipHandle zombie_groan = 0;
    AudioClipHandle zombie_step = 0;
};

struct EnemyAudioRuntime
{
    C_EnemyAIInfo::State last_state = C_EnemyAIInfo::Idle;
    float alert_cooldown_timer = 0.0f;
    float footstep_timer = 0.08f;
    AudioClipHandle groan_handle = 0;
    bool groan_loop_active = false;
};

static constexpr float kZombieAlertCooldown = 1.25f;
static constexpr float kZombieGroanMinDistance = 1.5f;
static constexpr float kZombieGroanMaxDistance = 5.0f;

struct GameAudio
{
    AudioSystem system = {};
    GameplayAudioClips clips = {};

    float player_footstep_timer = 0.08f;
    bool player_was_reloading = false;
    EntityID player_weapon_entity = NULL_ENTITY;
    int player_last_bullets = -1;
    std::unordered_map<EntityID, EnemyAudioRuntime> enemy_runtime = {};
};

static GameplayAudioClips LoadGameplayAudio(AudioSystem* audio_system)
{
    GameplayAudioClips clips = {};

    clips.soundtrack_loop = AudioSystem_LoadClipEx(
        audio_system,
        "gameplay_soundtrack_loop",
        "assets/sounds/All_Sounds_MP3_UNMASTERED/Bad_Signs.mp3",
        AUDIO_CLIP_CATEGORY_SOUNDTRACK);
    clips.weapon_fire = AudioSystem_LoadClipEx(
        audio_system,
        "weapon_fire",
        "assets/sounds/game_sound/weapon/weapon_fired.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.weapon_dry_fire = 0;
    clips.weapon_reload = AudioSystem_LoadClipEx(
        audio_system,
        "weapon_reload",
        "assets/sounds/game_sound/weapon/weapon_cocking.mp3",
        AUDIO_CLIP_CATEGORY_SFX);
    clips.footstep = 0;
    clips.zombie_alert = AudioSystem_LoadClipEx(
        audio_system,
        "zombie_alert",
        "assets/sounds/game_sound/zombie/zombie_spotted.wav",
        AUDIO_CLIP_CATEGORY_SFX);
    // clips.zombie_groan = AudioSystem_LoadClipEx(
    //     audio_system,
    //     "zombie_groan",
    //     "assets/sounds/game_sound/zombie/zombie_wandering.wav",
    //     AUDIO_CLIP_CATEGORY_SFX);
    clips.zombie_groan = 0;
    clips.zombie_step = 0;

    if (clips.soundtrack_loop == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load 'gameplay_soundtrack_loop' from 'assets/sounds/All_Sounds_MP3_UNMASTERED/Bad_Signs.mp3'.");
    }
    if (clips.weapon_fire == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load 'weapon_fire' from 'assets/sounds/game_sound/weapon/weapon_fired.mp3'.");
    }
    if (clips.zombie_alert == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load 'zombie_alert' from 'assets/sounds/game_sound/zombie/zombie_spotted.mp3'.");
    }
    // if (clips.zombie_groan == 0)
    // {
    //     SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load 'zombie_groan' from 'assets/sounds/game_sound/zombie/zombie_wandering.wav'.");
    // }
    if (clips.weapon_reload == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: failed to load 'weapon_reload' from 'assets/sounds/game_sound/weapon/weapon_cocking.wav'.");
    }

    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SOUNDTRACK, 0.55f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_AMBIENT, 0.0f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SFX, 1.0f);

        AudioSystem_SetSoundtrackEnabled(audio_system, 0);

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

static void StopZombieGroanLoop(GameAudio* audio, EnemyAudioRuntime& runtime)
{
    if (runtime.groan_handle != 0 && runtime.groan_loop_active)
    {
        AudioSystem_StopClip(&audio->system, runtime.groan_handle);
    }

    runtime.groan_loop_active = false;
}

static bool IsZombieAudioEntityActive(ECS& ecs, EntityID entity)
{
    if (entity == NULL_ENTITY ||
        !ecs.IsEntityValid(entity) ||
        !ecs.Has<C_EnemyAIInfo>(entity) ||
        !ecs.Has<C_Faction>(entity) ||
        !ecs.Has<C_CombatInfo>(entity) ||
        ecs.GetComponent<C_Faction>(entity).type != FactionType::Zombie ||
        ecs.Has<C_DespawnTimer>(entity))
    {
        return false;
    }

    const C_CombatInfo& combat_info = ecs.GetComponent<C_CombatInfo>(entity);
    const C_EnemyAIInfo& enemy_info = ecs.GetComponent<C_EnemyAIInfo>(entity);
    return !combat_info.isDead && enemy_info.currentState != C_EnemyAIInfo::Dead;
}

static AudioClipHandle EnsureZombieGroanHandle(GameAudio* audio, EntityID entity, EnemyAudioRuntime& runtime)
{
    if (audio->clips.zombie_groan == 0)
    {
        return 0;
    }

    if (runtime.groan_handle != 0)
    {
        return runtime.groan_handle;
    }

    char logical_name[64] = {};
    SDL_snprintf(logical_name, sizeof(logical_name), "zombie_groan_loop_%u", entity);
    runtime.groan_handle = AudioSystem_CloneClip(&audio->system, audio->clips.zombie_groan, logical_name);
    return runtime.groan_handle;
}

static bool CanZombieEmitGroan(ECS& ecs, EntityID entity, const C_EnemyAIInfo& info, const C_Faction& faction)
{
    return faction.type == FactionType::Zombie &&
        info.currentState != C_EnemyAIInfo::Patrol &&
        IsZombieAudioEntityActive(ecs, entity);
}

static void ApplyUISoundtrackMix(GameAudio* audio, GameState state)
{
    AudioSystem* audio_system = &audio->system;
    const bool should_play_soundtrack = state == GameState::MainMenu;

    if (!should_play_soundtrack)
    {
        if (AudioSystem_GetSoundtrackEnabled(audio_system))
        {
            AudioSystem_SetSoundtrackEnabled(audio_system, 0);
        }
        return;
    }

    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SOUNDTRACK, 0.50f);

    if (audio->clips.soundtrack_loop == 0)
    {
        AudioSystem_SetSoundtrackEnabled(audio_system, 0);
        return;
    }

    if (AudioSystem_GetSoundtrackEnabled(audio_system))
    {
        return;
    }

    AudioSystem_SetSoundtrackEnabled(audio_system, 1);
    if (!AudioSystem_PlaySoundtrackLoop(audio_system, audio->clips.soundtrack_loop, 1.0f))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GameplayAudio: soundtrack loaded but did not start.");
        AudioSystem_SetSoundtrackEnabled(audio_system, 0);
    }
}

static void CleanupEnemyAudioRuntime(GameAudio* audio, ECS& ecs)
{
    for (auto it = audio->enemy_runtime.begin(); it != audio->enemy_runtime.end();)
    {
        if (!IsZombieAudioEntityActive(ecs, it->first))
        {
            StopZombieGroanLoop(audio, it->second);
            it = audio->enemy_runtime.erase(it);
            continue;
        }
        ++it;
    }
}

static void UpdatePlayerMovementAudio(
    GameAudio* audio,
    Scene& scene,
    float dt,
    bool gameplay_audio_enabled)
{
    if (!gameplay_audio_enabled || audio->clips.footstep == 0)
    {
        audio->player_footstep_timer = 0.08f;
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

            if (!move_info.isGrounded || !move_info.isMoving)
            {
                audio->player_footstep_timer = 0.08f;
                return;
            }

            audio->player_footstep_timer -= dt;
            if (audio->player_footstep_timer > 0.0f)
            {
                return;
            }

            float interval = 0.48f;
            if (move_info.state == MoveState::Sprint)
            {
                interval = 0.32f;
            }
            else if (move_info.state == MoveState::Crouch)
            {
                interval = 0.68f;
            }

            PlayGameplaySFXAt(&audio->system, audio->clips.footstep, 0.34f, position, 0.75f, 18.0f);
            audio->player_footstep_timer = interval;
        });

    if (!processed_player)
    {
        audio->player_footstep_timer = 0.08f;
    }
}

static void UpdatePlayerCombatAudio(
    GameAudio* audio,
    Scene& scene,
    bool gameplay_audio_enabled)
{
    ECS& ecs = scene.GetECS();
    bool is_reloading = false;

    if (!gameplay_audio_enabled)
    {
        audio->player_was_reloading = false;
        audio->player_weapon_entity = NULL_ENTITY;
        audio->player_last_bullets = -1;
        return;
    }

    bool processed_player = false;
    ecs.GetView<C_PlayerInput, C_CombatInfo, C_WeaponSocket>().ForEach(
        [&](C_PlayerInput&, C_CombatInfo& combat_info, C_WeaponSocket& weapon_socket)
        {
            if (processed_player)
            {
                return;
            }

            processed_player = true;
            is_reloading = combat_info.isReloading;

            if (!weapon_socket.equipped ||
                weapon_socket.weapon_entity == NULL_ENTITY ||
                !ecs.IsEntityValid(weapon_socket.weapon_entity) ||
                !ecs.Has<C_WeaponRanged>(weapon_socket.weapon_entity))
            {
                audio->player_weapon_entity = NULL_ENTITY;
                audio->player_last_bullets = -1;
                return;
            }

            C_WeaponRanged& weapon = ecs.GetComponent<C_WeaponRanged>(weapon_socket.weapon_entity);
            if (audio->player_weapon_entity != weapon_socket.weapon_entity)
            {
                audio->player_weapon_entity = weapon_socket.weapon_entity;
                audio->player_last_bullets = weapon.currentBullets;
                return;
            }

            if (weapon.currentBullets < audio->player_last_bullets)
            {
                PlayGameplaySFX(&audio->system, audio->clips.weapon_fire, 1.0f);
            }

            audio->player_last_bullets = weapon.currentBullets;
        });

    if (!processed_player)
    {
        audio->player_weapon_entity = NULL_ENTITY;
        audio->player_last_bullets = -1;
    }

    if (is_reloading && !audio->player_was_reloading)
    {
        PlayGameplaySFX(&audio->system, audio->clips.weapon_reload, 1.0f);
    }

    audio->player_was_reloading = is_reloading;
}

static void UpdateZombieAudio(
    GameAudio* audio,
    Scene& scene,
    float dt,
    bool gameplay_audio_enabled)
{
    ECS& ecs = scene.GetECS();
    CleanupEnemyAudioRuntime(audio, ecs);

    if (!gameplay_audio_enabled)
    {
        for (auto& entry : audio->enemy_runtime)
        {
            StopZombieGroanLoop(audio, entry.second);
        }
        return;
    }

    ecs.GetView<C_Transform, C_EnemyAIInfo, C_Faction>().ForEach(
        [&](EntityID entity, C_Transform& transform, C_EnemyAIInfo& info, C_Faction& faction)
        {
            if (faction.type != FactionType::Zombie || !IsZombieAudioEntityActive(ecs, entity))
            {
                return;
            }

            EnemyAudioRuntime& runtime = audio->enemy_runtime[entity];
            runtime.alert_cooldown_timer -= dt;
            if (runtime.alert_cooldown_timer < 0.0f)
            {
                runtime.alert_cooldown_timer = 0.0f;
            }

            if (info.currentState == runtime.last_state)
            {
                return;
            }

            const bool became_threat =
                info.currentState == C_EnemyAIInfo::Alerted;

            if (audio->clips.zombie_alert != 0 && became_threat && runtime.alert_cooldown_timer <= 0.0f)
            {
                const glm::vec3 position = glm::vec3(transform.matrix[3]);
                PlayGameplaySFXAt(&audio->system, audio->clips.zombie_alert, 0.65f, position, 1.0f, 5.0f);
                runtime.alert_cooldown_timer = kZombieAlertCooldown;
            }

            runtime.last_state = info.currentState;
        });

    ecs.GetView<C_Transform, C_EnemyAIInfo, C_MovementInfo, C_Faction>().ForEach(
        [&](EntityID entity, C_Transform& transform, C_EnemyAIInfo& info, C_MovementInfo& move_info, C_Faction& faction)
        {
            if (faction.type != FactionType::Zombie || !IsZombieAudioEntityActive(ecs, entity))
            {
                return;
            }

            EnemyAudioRuntime& runtime = audio->enemy_runtime[entity];
            const bool can_make_steps =
                info.currentState != C_EnemyAIInfo::Dead &&
                info.currentState != C_EnemyAIInfo::Attack &&
                move_info.isGrounded &&
                move_info.isMoving;

            if (!can_make_steps)
            {
                runtime.footstep_timer = 0.08f;
                return;
            }

            runtime.footstep_timer -= dt;
            if (runtime.footstep_timer > 0.0f)
            {
                return;
            }

            const glm::vec3 position = glm::vec3(transform.matrix[3]);
            const bool chasing = info.currentState == C_EnemyAIInfo::Chase;
            PlayGameplaySFXAt(&audio->system, audio->clips.zombie_step, chasing ? 0.38f : 0.26f, position, 1.0f, 5.0f);
            runtime.footstep_timer = chasing ? 0.45f : 0.62f;
        });

    ecs.GetView<C_Transform, C_EnemyAIInfo, C_Faction>().ForEach(
        [&](EntityID entity, C_Transform& transform, C_EnemyAIInfo& info, C_Faction& faction)
        {
            EnemyAudioRuntime& runtime = audio->enemy_runtime[entity];
            if (!CanZombieEmitGroan(ecs, entity, info, faction))
            {
                StopZombieGroanLoop(audio, runtime);
                return;
            }

            const AudioClipHandle groan_handle = EnsureZombieGroanHandle(audio, entity, runtime);
            if (groan_handle == 0)
            {
                return;
            }

            const glm::vec3 position = glm::vec3(transform.matrix[3]);
            AudioSystem_SetClipSpatialized(&audio->system, groan_handle, 1);
            AudioSystem_SetClipPosition(&audio->system, groan_handle, position.x, position.y, position.z);
            AudioSystem_SetClipMinMaxDistance(&audio->system, groan_handle, kZombieGroanMinDistance, kZombieGroanMaxDistance);

            const float volume = (info.currentState == C_EnemyAIInfo::Chase) ? 0.58f : 0.38f;
            AudioSystem_SetClipVolume(&audio->system, groan_handle, volume);

            if (!runtime.groan_loop_active)
            {
                if (AudioSystem_PlaySpatialLoop(&audio->system, groan_handle, volume, position.x, position.y, position.z))
                {
                    runtime.groan_loop_active = true;
                }
            }
        });
}

GameAudio* GameAudio_Init()
{
    GameAudio* audio = new GameAudio();
    audio->system = AudioSystem_Create((AudioSystemCreateInfo){
        .debug_name = "GameAudio",
        .initial_capacity = 64,
        .master_volume = 1.0f
    });
    AudioSystem_LogSummary(&audio->system);
    audio->clips = LoadGameplayAudio(&audio->system);
    return audio;
}

void GameAudio_Destroy(GameAudio* audio)
{
    if (audio == nullptr)
    {
        return;
    }

    AudioSystem_Destroy(&audio->system);
    delete audio;
}

void GameAudio_Update(
    GameAudio* audio,
    Scene& scene,
    const CameraInfo& listener_camera,
    float dt)
{
    if (audio == nullptr)
    {
        return;
    }

    const GameState current_ui_state = GameUI_GetState();
    const bool current_skill_choice_open = GameUI_IsLevelStartSkillSelectionOpen();
    const bool gameplay_world_audio_enabled =
        current_ui_state == GameState::Playing &&
        !current_skill_choice_open;

    ApplyUISoundtrackMix(audio, current_ui_state);
    UpdatePlayerCombatAudio(audio, scene, gameplay_world_audio_enabled);
    UpdatePlayerMovementAudio(audio, scene, dt, gameplay_world_audio_enabled);
    UpdateZombieAudio(audio, scene, dt, gameplay_world_audio_enabled);

    const glm::vec3 listener_pos = listener_camera.position;
    const glm::vec3 listener_forward = glm::normalize(glm::vec3(
        -listener_camera.view[0][2],
        -listener_camera.view[1][2],
        -listener_camera.view[2][2]));
    const glm::vec3 listener_up = glm::normalize(glm::vec3(
        listener_camera.view[0][1],
        listener_camera.view[1][1],
        listener_camera.view[2][1]));

    AudioSystem_UpdateSpatialState(
        &audio->system,
        listener_pos.x, listener_pos.y, listener_pos.z,
        listener_forward.x, listener_forward.y, listener_forward.z,
        listener_up.x, listener_up.y, listener_up.z,
        nullptr, 0);

    AudioSystem_Update(&audio->system, dt);
}