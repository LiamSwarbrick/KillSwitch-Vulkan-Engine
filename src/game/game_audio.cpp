#include "game_audio.h"

#include "SDL3/SDL.h"

#include <cmath>
#include <memory>
#include <new>

namespace
{
constexpr float kSoundtrackFadeSeconds = 0.8f;
constexpr float kMenuSoundtrackVolume = 0.28f;
constexpr float kGameplaySoundtrackVolume = 0.22f;
constexpr float kGameplayAmbientVolume = 0.18f;
constexpr float kSpatialMinDistance = 1.0f;
constexpr float kSpatialMaxDistance = 28.0f;

enum class LocomotionCue
{
    Idle,
    Walk,
    Sprint,
    Crouch,
};

struct GameAudioCues
{
    AudioClipHandle menu_soundtrack = 0;
    AudioClipHandle gameplay_soundtrack = 0;
    AudioClipHandle gameplay_ambient = 0;

    AudioClipHandle walk_step = 0;
    AudioClipHandle sprint_step = 0;
    AudioClipHandle crouch_step = 0;
    AudioClipHandle jump = 0;
    AudioClipHandle land = 0;
    AudioClipHandle aim_in = 0;
    AudioClipHandle aim_out = 0;
    AudioClipHandle pause_open = 0;
    AudioClipHandle pause_close = 0;

    AudioClipHandle camera_toggle = 0;
    AudioClipHandle melee_swing = 0;
    AudioClipHandle melee_hit = 0;
    AudioClipHandle revolver_fire = 0;
    AudioClipHandle revolver_hit = 0;
    AudioClipHandle revolver_miss = 0;
    AudioClipHandle reload_start = 0;
    AudioClipHandle reload_complete = 0;
    AudioClipHandle dry_fire = 0;
    AudioClipHandle upgrade_open = 0;
    AudioClipHandle upgrade_ammo = 0;
    AudioClipHandle upgrade_piercing = 0;
    AudioClipHandle upgrade_health = 0;
    AudioClipHandle ammo_pickup = 0;
    AudioClipHandle health_pickup = 0;
    AudioClipHandle floor_start = 0;
    AudioClipHandle floor_advance = 0;
    AudioClipHandle zombie_ranged = 0;
    AudioClipHandle zombie_death = 0;
};

struct GameAudioImpl
{
    GameAudioCues cues = {};
    GameState last_game_state = GameState::MainMenu;
    bool has_game_state = false;
    bool was_aiming = false;
    bool has_ground_state = false;
    bool was_grounded = false;
    float footstep_timer = 0.0f;
    LocomotionCue last_locomotion = LocomotionCue::Idle;
};

static GameAudioImpl*
get_impl(GameAudio* game_audio)
{
    if (game_audio == nullptr)
    {
        return nullptr;
    }

    return static_cast<GameAudioImpl*>(game_audio->impl);
}

static AudioClipHandle
load_clip(GameAudio* game_audio, const char* logical_name, const char* path, AudioClipCategory category)
{
    if (game_audio == nullptr || game_audio->audio_system == nullptr)
    {
        return 0;
    }

    return AudioSystem_LoadClipEx(game_audio->audio_system, logical_name, path, category);
}

static void
load_cues(GameAudio* game_audio)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (impl == nullptr)
    {
        return;
    }

    GameAudioCues& cues = impl->cues;
    cues.menu_soundtrack       = load_clip(game_audio, "menu_soundtrack",       "All_Sounds_MP3_UNMASTERED2/Bad_Signs.mp3",                 AUDIO_CLIP_CATEGORY_SOUNDTRACK);
    cues.gameplay_soundtrack   = load_clip(game_audio, "gameplay_soundtrack",   "All_Sounds_MP3_UNMASTERED2/Deep_Forest_Steady_Drone.mp3", AUDIO_CLIP_CATEGORY_SOUNDTRACK);
    cues.gameplay_ambient      = load_clip(game_audio, "gameplay_ambient",      "All_Sounds_MP3_UNMASTERED2/Low_Winds.mp3",                AUDIO_CLIP_CATEGORY_AMBIENT);

    cues.walk_step             = load_clip(game_audio, "walk_step",             "All_Sounds_MP3_UNMASTERED/Low_Noise_Floor.mp3",           AUDIO_CLIP_CATEGORY_SFX);
    cues.sprint_step           = load_clip(game_audio, "sprint_step",           "All_Sounds_MP3_UNMASTERED/TV_Static.mp3",                 AUDIO_CLIP_CATEGORY_SFX);
    cues.crouch_step           = load_clip(game_audio, "crouch_step",           "All_Sounds_MP3_UNMASTERED/Intercom.mp3",                  AUDIO_CLIP_CATEGORY_SFX);
    cues.jump                  = load_clip(game_audio, "jump",                  "All_Sounds_MP3_UNMASTERED/Broken_Microwave.mp3",          AUDIO_CLIP_CATEGORY_SFX);
    cues.land                  = load_clip(game_audio, "land",                  "All_Sounds_MP3_UNMASTERED/Deep_Thunderous_Drone.mp3",     AUDIO_CLIP_CATEGORY_SFX);
    cues.aim_in                = load_clip(game_audio, "aim_in",                "All_Sounds_MP3_UNMASTERED/Echo_Beach.mp3",                AUDIO_CLIP_CATEGORY_SFX);
    cues.aim_out               = load_clip(game_audio, "aim_out",               "All_Sounds_MP3_UNMASTERED/Echo_Beach2.mp3",               AUDIO_CLIP_CATEGORY_SFX);
    cues.pause_open            = load_clip(game_audio, "pause_open",            "All_Sounds_MP3_UNMASTERED/Public_Access_Channel.mp3",     AUDIO_CLIP_CATEGORY_SFX);
    cues.pause_close           = load_clip(game_audio, "pause_close",           "All_Sounds_MP3_UNMASTERED2/Public_Access_Channel.mp3",    AUDIO_CLIP_CATEGORY_SFX);

    cues.camera_toggle         = load_clip(game_audio, "camera_toggle",         "All_Sounds_MP3_UNMASTERED/Constantinople.mp3",            AUDIO_CLIP_CATEGORY_SFX);
    cues.melee_swing           = load_clip(game_audio, "melee_swing",           "All_Sounds_MP3_UNMASTERED/drive_by.mp3",                  AUDIO_CLIP_CATEGORY_SFX);
    cues.melee_hit             = load_clip(game_audio, "melee_hit",             "All_Sounds_MP3_UNMASTERED/Biting_Cold.mp3",               AUDIO_CLIP_CATEGORY_SFX);
    cues.revolver_fire         = load_clip(game_audio, "revolver_fire",         "All_Sounds_MP3_UNMASTERED2/Broken_Microwave.mp3",         AUDIO_CLIP_CATEGORY_SFX);
    cues.revolver_hit          = load_clip(game_audio, "revolver_hit",          "All_Sounds_MP3_UNMASTERED2/TV_Static.mp3",                AUDIO_CLIP_CATEGORY_SFX);
    cues.revolver_miss         = load_clip(game_audio, "revolver_miss",         "All_Sounds_MP3_UNMASTERED2/drive_by.mp3",                 AUDIO_CLIP_CATEGORY_SFX);
    cues.reload_start          = load_clip(game_audio, "reload_start",          "All_Sounds_MP3_UNMASTERED2/Intercom.mp3",                 AUDIO_CLIP_CATEGORY_SFX);
    cues.reload_complete       = load_clip(game_audio, "reload_complete",       "All_Sounds_MP3_UNMASTERED2/pretty_lights.mp3",            AUDIO_CLIP_CATEGORY_SFX);
    cues.dry_fire              = load_clip(game_audio, "dry_fire",              "All_Sounds_MP3_UNMASTERED2/powerlines.mp3",               AUDIO_CLIP_CATEGORY_SFX);
    cues.upgrade_open          = load_clip(game_audio, "upgrade_open",          "All_Sounds_MP3_UNMASTERED/Cats_Cradle.mp3",               AUDIO_CLIP_CATEGORY_SFX);
    cues.upgrade_ammo          = load_clip(game_audio, "upgrade_ammo",          "All_Sounds_MP3_UNMASTERED/pretty_lights.mp3",             AUDIO_CLIP_CATEGORY_SFX);
    cues.upgrade_piercing      = load_clip(game_audio, "upgrade_piercing",      "All_Sounds_MP3_UNMASTERED/Deep_Thunderous_Drone.mp3",     AUDIO_CLIP_CATEGORY_SFX);
    cues.upgrade_health        = load_clip(game_audio, "upgrade_health",        "All_Sounds_MP3_UNMASTERED/Echo_Beach2.mp3",               AUDIO_CLIP_CATEGORY_SFX);
    cues.ammo_pickup           = load_clip(game_audio, "ammo_pickup",           "All_Sounds_MP3_UNMASTERED2/pretty_lights.mp3",            AUDIO_CLIP_CATEGORY_SFX);
    cues.health_pickup         = load_clip(game_audio, "health_pickup",         "All_Sounds_MP3_UNMASTERED2/Echo_Beach2.mp3",              AUDIO_CLIP_CATEGORY_SFX);
    cues.floor_start           = load_clip(game_audio, "floor_start",           "All_Sounds_MP3_UNMASTERED/Silent_Film_Star.mp3",          AUDIO_CLIP_CATEGORY_SFX);
    cues.floor_advance         = load_clip(game_audio, "floor_advance",         "All_Sounds_MP3_UNMASTERED2/Silent_Film_Star.mp3",         AUDIO_CLIP_CATEGORY_SFX);
    cues.zombie_ranged         = load_clip(game_audio, "zombie_ranged",         "All_Sounds_MP3_UNMASTERED/powerlines.mp3",                AUDIO_CLIP_CATEGORY_SFX);
    cues.zombie_death          = load_clip(game_audio, "zombie_death",          "All_Sounds_MP3_UNMASTERED2/Biting_Cold.mp3",              AUDIO_CLIP_CATEGORY_SFX);
}

static void
play_one_shot(GameAudio* game_audio, AudioClipHandle handle, float volume)
{
    if (game_audio == nullptr || game_audio->audio_system == nullptr || handle == 0)
    {
        return;
    }

    (void)AudioSystem_PlaySFXOneShot(game_audio->audio_system, handle, volume);
}

static void
play_spatial_one_shot(GameAudio* game_audio, AudioClipHandle handle, float volume, const glm::vec3& world_position)
{
    if (game_audio == nullptr || game_audio->audio_system == nullptr || handle == 0)
    {
        return;
    }

    (void)AudioSystem_PlayEntitySFXAt(
        game_audio->audio_system,
        handle,
        volume,
        world_position.x,
        world_position.y,
        world_position.z,
        kSpatialMinDistance,
        kSpatialMaxDistance
    );
}

static void
ensure_menu_mix(GameAudio* game_audio)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (impl == nullptr || game_audio->audio_system == nullptr)
    {
        return;
    }

    if (impl->cues.gameplay_ambient != 0)
    {
        AudioSystem_StopClip(game_audio->audio_system, impl->cues.gameplay_ambient);
    }

    if (impl->cues.menu_soundtrack != 0 &&
        !AudioSystem_IsClipPlaying(game_audio->audio_system, impl->cues.menu_soundtrack))
    {
        (void)AudioSystem_PlaySoundtrackFaded(
            game_audio->audio_system,
            impl->cues.menu_soundtrack,
            kMenuSoundtrackVolume,
            kSoundtrackFadeSeconds
        );
    }
}

static void
ensure_gameplay_mix(GameAudio* game_audio)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (impl == nullptr || game_audio->audio_system == nullptr)
    {
        return;
    }

    if (impl->cues.gameplay_soundtrack != 0 &&
        !AudioSystem_IsClipPlaying(game_audio->audio_system, impl->cues.gameplay_soundtrack))
    {
        (void)AudioSystem_PlaySoundtrackFaded(
            game_audio->audio_system,
            impl->cues.gameplay_soundtrack,
            kGameplaySoundtrackVolume,
            kSoundtrackFadeSeconds
        );
    }

    if (impl->cues.gameplay_ambient != 0 &&
        !AudioSystem_IsClipPlaying(game_audio->audio_system, impl->cues.gameplay_ambient))
    {
        (void)AudioSystem_PlayAmbientLoop(
            game_audio->audio_system,
            impl->cues.gameplay_ambient,
            kGameplayAmbientVolume
        );
    }
}

static LocomotionCue
compute_locomotion(const GameAudioUpdateContext& context)
{
    if (!context.player_grounded || !context.player_moving)
    {
        return LocomotionCue::Idle;
    }

    if (context.player_crouching)
    {
        return LocomotionCue::Crouch;
    }

    if (context.player_sprinting)
    {
        return LocomotionCue::Sprint;
    }

    return LocomotionCue::Walk;
}

static float
step_interval_for(const GameAudioUpdateContext& context, LocomotionCue locomotion)
{
    const float horizontal_speed = std::sqrt(
        context.player_velocity.x * context.player_velocity.x +
        context.player_velocity.z * context.player_velocity.z
    );

    switch (locomotion)
    {
        case LocomotionCue::Sprint: return horizontal_speed > 4.0f ? 0.22f : 0.28f;
        case LocomotionCue::Crouch: return 0.62f;
        case LocomotionCue::Walk:   return horizontal_speed > 2.5f ? 0.34f : 0.42f;
        default:                    return 0.0f;
    }
}

static AudioClipHandle
step_handle_for(const GameAudioImpl& impl, LocomotionCue locomotion)
{
    switch (locomotion)
    {
        case LocomotionCue::Sprint: return impl.cues.sprint_step;
        case LocomotionCue::Crouch: return impl.cues.crouch_step;
        case LocomotionCue::Walk:   return impl.cues.walk_step;
        default:                    return 0;
    }
}

static void
emit_event_internal(GameAudio* game_audio, GameAudioEvent event, const glm::vec3* world_position)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (impl == nullptr)
    {
        return;
    }

    AudioClipHandle handle = 0;
    float volume = 0.35f;
    bool spatialized = false;

    switch (event)
    {
        case GameAudioEvent::CameraShoulderToggle:
            handle = impl->cues.camera_toggle;
            volume = 0.18f;
            break;
        case GameAudioEvent::MeleeSwing:
            handle = impl->cues.melee_swing;
            volume = 0.24f;
            break;
        case GameAudioEvent::MeleeHit:
            handle = impl->cues.melee_hit;
            volume = 0.28f;
            spatialized = true;
            break;
        case GameAudioEvent::RevolverFire:
            handle = impl->cues.revolver_fire;
            volume = 0.38f;
            break;
        case GameAudioEvent::RevolverHit:
            handle = impl->cues.revolver_hit;
            volume = 0.24f;
            spatialized = true;
            break;
        case GameAudioEvent::RevolverMiss:
            handle = impl->cues.revolver_miss;
            volume = 0.18f;
            break;
        case GameAudioEvent::ReloadStart:
            handle = impl->cues.reload_start;
            volume = 0.20f;
            break;
        case GameAudioEvent::ReloadComplete:
            handle = impl->cues.reload_complete;
            volume = 0.18f;
            break;
        case GameAudioEvent::DryFire:
            handle = impl->cues.dry_fire;
            volume = 0.16f;
            break;
        case GameAudioEvent::UpgradeScreenOpen:
            handle = impl->cues.upgrade_open;
            volume = 0.22f;
            break;
        case GameAudioEvent::UpgradeAmmoSelected:
            handle = impl->cues.upgrade_ammo;
            volume = 0.20f;
            break;
        case GameAudioEvent::UpgradePiercingSelected:
            handle = impl->cues.upgrade_piercing;
            volume = 0.20f;
            break;
        case GameAudioEvent::UpgradeHealthSelected:
            handle = impl->cues.upgrade_health;
            volume = 0.22f;
            break;
        case GameAudioEvent::AmmoPickup:
            handle = impl->cues.ammo_pickup;
            volume = 0.24f;
            spatialized = true;
            break;
        case GameAudioEvent::HealthPickup:
            handle = impl->cues.health_pickup;
            volume = 0.24f;
            spatialized = true;
            break;
        case GameAudioEvent::FloorStart:
            handle = impl->cues.floor_start;
            volume = 0.26f;
            break;
        case GameAudioEvent::FloorAdvance:
            handle = impl->cues.floor_advance;
            volume = 0.28f;
            break;
        case GameAudioEvent::ZombieRangedAttack:
            handle = impl->cues.zombie_ranged;
            volume = 0.24f;
            spatialized = true;
            break;
        case GameAudioEvent::ZombieDeath:
            handle = impl->cues.zombie_death;
            volume = 0.24f;
            spatialized = true;
            break;
    }

    if (handle == 0)
    {
        return;
    }

    if (spatialized && world_position != nullptr)
    {
        play_spatial_one_shot(game_audio, handle, volume, *world_position);
        return;
    }

    play_one_shot(game_audio, handle, volume);
}
}

bool
GameAudio_Init(GameAudio* game_audio, AudioSystem* audio_system)
{
    if (game_audio == nullptr || audio_system == nullptr)
    {
        return false;
    }

    std::unique_ptr<GameAudioImpl> impl(new (std::nothrow) GameAudioImpl());
    if (!impl)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "GameAudio: failed to allocate runtime state.");
        return false;
    }

    game_audio->audio_system = audio_system;
    game_audio->impl = impl.release();

    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SOUNDTRACK, 0.85f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_AMBIENT, 0.75f);
    AudioSystem_SetCategoryVolume(audio_system, AUDIO_CLIP_CATEGORY_SFX, 0.90f);

    load_cues(game_audio);
    return true;
}

void
GameAudio_Shutdown(GameAudio* game_audio)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (game_audio == nullptr || game_audio->audio_system == nullptr || impl == nullptr)
    {
        return;
    }

    AudioSystem_StopClip(game_audio->audio_system, impl->cues.menu_soundtrack);
    AudioSystem_StopClip(game_audio->audio_system, impl->cues.gameplay_soundtrack);
    AudioSystem_StopClip(game_audio->audio_system, impl->cues.gameplay_ambient);

    delete impl;
    game_audio->impl = nullptr;
    game_audio->audio_system = nullptr;
}

void
GameAudio_Update(GameAudio* game_audio, const GameAudioUpdateContext& context)
{
    GameAudioImpl* impl = get_impl(game_audio);
    if (game_audio == nullptr || game_audio->audio_system == nullptr || impl == nullptr)
    {
        return;
    }

    switch (context.game_state)
    {
        case GameState::MainMenu:
        case GameState::Quitting:
            ensure_menu_mix(game_audio);
            break;
        case GameState::Playing:
        case GameState::Paused:
            ensure_gameplay_mix(game_audio);
            break;
    }

    if (!impl->has_game_state)
    {
        impl->last_game_state = context.game_state;
        impl->has_game_state = true;
    }
    else if (impl->last_game_state != context.game_state)
    {
        if (impl->last_game_state == GameState::MainMenu && context.game_state == GameState::Playing)
        {
            emit_event_internal(game_audio, GameAudioEvent::FloorStart, nullptr);
        }
        else if (context.game_state == GameState::Paused)
        {
            play_one_shot(game_audio, impl->cues.pause_open, 0.16f);
        }
        else if (impl->last_game_state == GameState::Paused && context.game_state == GameState::Playing)
        {
            play_one_shot(game_audio, impl->cues.pause_close, 0.14f);
        }

        impl->last_game_state = context.game_state;
    }

    if (context.game_state != GameState::Playing)
    {
        impl->was_aiming = false;
        impl->footstep_timer = 0.0f;
        impl->last_locomotion = LocomotionCue::Idle;
        impl->has_ground_state = false;
        return;
    }

    if (context.jump_just_pressed && context.player_grounded)
    {
        play_one_shot(game_audio, impl->cues.jump, 0.20f);
    }

    if (context.aiming && !impl->was_aiming)
    {
        play_one_shot(game_audio, impl->cues.aim_in, 0.14f);
    }
    else if (!context.aiming && impl->was_aiming)
    {
        play_one_shot(game_audio, impl->cues.aim_out, 0.12f);
    }
    impl->was_aiming = context.aiming;

    if (impl->has_ground_state)
    {
        if (context.player_grounded && !impl->was_grounded)
        {
            play_one_shot(game_audio, impl->cues.land, 0.16f);
        }
    }
    else
    {
        impl->has_ground_state = true;
    }
    impl->was_grounded = context.player_grounded;

    const LocomotionCue locomotion = compute_locomotion(context);
    if (locomotion != impl->last_locomotion)
    {
        impl->footstep_timer = 0.0f;
        impl->last_locomotion = locomotion;
    }

    if (locomotion == LocomotionCue::Idle)
    {
        return;
    }

    impl->footstep_timer -= context.dt;
    if (impl->footstep_timer > 0.0f)
    {
        return;
    }

    const AudioClipHandle step_handle = step_handle_for(*impl, locomotion);
    const float volume = (locomotion == LocomotionCue::Sprint) ? 0.18f : 0.12f;
    play_one_shot(game_audio, step_handle, volume);
    impl->footstep_timer = step_interval_for(context, locomotion);
}

void
GameAudio_EmitEvent(GameAudio* game_audio, GameAudioEvent event)
{
    emit_event_internal(game_audio, event, nullptr);
}

void
GameAudio_EmitEventAt(GameAudio* game_audio, GameAudioEvent event, const glm::vec3& world_position)
{
    emit_event_internal(game_audio, event, &world_position);
}
