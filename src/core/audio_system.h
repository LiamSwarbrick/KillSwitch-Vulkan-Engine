#ifndef ENGINE_AUDIO_SYSTEM_H
#define ENGINE_AUDIO_SYSTEM_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_NAME_MAX_LEN 96
#define AUDIO_PATH_MAX_LEN 260

typedef u32 AudioClipHandle;

typedef enum AudioClipCategory
{
    AUDIO_CLIP_CATEGORY_SFX = 0,
    AUDIO_CLIP_CATEGORY_AMBIENT,
    AUDIO_CLIP_CATEGORY_SOUNDTRACK
}
AudioClipCategory;

typedef struct AudioSystemCreateInfo
{
    const char* debug_name;
    u32 initial_capacity;
    float master_volume;
}
AudioSystemCreateInfo;

typedef struct AudioSystem
{
    ThreadAllocTracker tt;
    void* impl;
}
AudioSystem;


// Lifecycle
AudioSystem AudioSystem_Create(AudioSystemCreateInfo create_info);
void AudioSystem_Destroy(AudioSystem* system);

// Asset loading / lookup
AudioClipHandle AudioSystem_FindClipByName(const AudioSystem* system, const char* logical_name);
AudioClipHandle AudioSystem_FindClipByPath(const AudioSystem* system, const char* source_path);

AudioClipHandle AudioSystem_LoadClip(
    AudioSystem* system,
    const char* logical_name,
    const char* source_path
);

AudioClipHandle AudioSystem_LoadClipEx(
    AudioSystem* system,
    const char* logical_name,
    const char* source_path,
    AudioClipCategory category
);

// Generic playback
b32 AudioSystem_PlayOneShot(AudioSystem* system, AudioClipHandle handle, float volume);
b32 AudioSystem_PlayLoop(AudioSystem* system, AudioClipHandle handle, float volume);
void AudioSystem_StopClip(AudioSystem* system, AudioClipHandle handle);
void AudioSystem_StopAll(AudioSystem* system);

// Category-oriented helpers
b32 AudioSystem_PlaySFXOneShot(AudioSystem* system, AudioClipHandle handle, float volume);
b32 AudioSystem_PlayAmbientLoop(AudioSystem* system, AudioClipHandle handle, float volume);
b32 AudioSystem_PlaySoundtrackLoop(AudioSystem* system, AudioClipHandle handle, float volume);

// State / controls
void AudioSystem_SetMasterVolume(AudioSystem* system, float volume);
float AudioSystem_GetMasterVolume(const AudioSystem* system);

void AudioSystem_SetCategoryVolume(AudioSystem* system, AudioClipCategory category, float volume);
float AudioSystem_GetCategoryVolume(const AudioSystem* system, AudioClipCategory category);

void AudioSystem_SetClipVolume(AudioSystem* system, AudioClipHandle handle, float volume);
float AudioSystem_GetClipVolume(const AudioSystem* system, AudioClipHandle handle);

void AudioSystem_SetClipLooping(AudioSystem* system, AudioClipHandle handle, b32 should_loop);
b32 AudioSystem_IsClipPlaying(const AudioSystem* system, AudioClipHandle handle);
void AudioSystem_PauseClip(AudioSystem* system, AudioClipHandle handle);
void AudioSystem_ResumeClip(AudioSystem* system, AudioClipHandle handle);

// Spatial audio (first pass)
void AudioSystem_SetListenerPosition(AudioSystem* system, float x, float y, float z);
void AudioSystem_SetClipSpatialized(AudioSystem* system, AudioClipHandle handle, b32 enabled);
void AudioSystem_SetClipPosition(AudioSystem* system, AudioClipHandle handle, float x, float y, float z);
void AudioSystem_SetClipMinMaxDistance(AudioSystem* system, AudioClipHandle handle, float min_distance, float max_distance);

b32 AudioSystem_PlaySpatialOneShot(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z
);

b32 AudioSystem_PlaySpatialLoop(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z
);

// Low-pass placeholders for future integration.
// These are state-only in this submission version.
void AudioSystem_SetClipLowPassEnabled(AudioSystem* system, AudioClipHandle handle, b32 enabled);
void AudioSystem_SetClipLowPassCutoff(AudioSystem* system, AudioClipHandle handle, float cutoff_hz);

// Debug
void AudioSystem_LogSummary(const AudioSystem* system);

#ifdef __cplusplus
}
#endif

#endif  // ENGINE_AUDIO_SYSTEM_H