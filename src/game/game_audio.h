#pragma once

#include "core/audio_system.h"
#include "game_ui.h"
#include "glm/glm.hpp"

enum class GameAudioEvent
{
    CameraShoulderToggle,
    MeleeSwing,
    MeleeHit,
    RevolverFire,
    RevolverHit,
    RevolverMiss,
    ReloadStart,
    ReloadComplete,
    DryFire,
    UpgradeScreenOpen,
    UpgradeAmmoSelected,
    UpgradePiercingSelected,
    UpgradeHealthSelected,
    AmmoPickup,
    HealthPickup,
    FloorStart,
    FloorAdvance,
    ZombieRangedAttack,
    ZombieDeath,
};

struct GameAudioUpdateContext
{
    float dt = 0.0f;
    GameState game_state = GameState::MainMenu;
    glm::vec3 player_position = glm::vec3(0.0f);
    glm::vec3 player_velocity = glm::vec3(0.0f);
    bool player_grounded = false;
    bool player_moving = false;
    bool player_sprinting = false;
    bool player_crouching = false;
    bool aiming = false;
    bool jump_just_pressed = false;
};

struct GameAudio
{
    AudioSystem* audio_system = nullptr;
    void* impl = nullptr;
};

bool GameAudio_Init(GameAudio* game_audio, AudioSystem* audio_system);
void GameAudio_Shutdown(GameAudio* game_audio);

void GameAudio_Update(GameAudio* game_audio, const GameAudioUpdateContext& context);
void GameAudio_EmitEvent(GameAudio* game_audio, GameAudioEvent event);
void GameAudio_EmitEventAt(GameAudio* game_audio, GameAudioEvent event, const glm::vec3& world_position);
