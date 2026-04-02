#ifndef ENGINE_AUDIO_SYSTEM_H
#define ENGINE_AUDIO_SYSTEM_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_NAME_MAX_LEN 96
#define AUDIO_PATH_MAX_LEN 260

typedef u32 AudioClipHandle;

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
AudioClipHandle AudioSystem_LoadClip(AudioSystem* system, const char* logical_name, const char* source_path);

// Playback
b32 AudioSystem_PlayOneShot(AudioSystem* system, AudioClipHandle handle, float volume);
b32 AudioSystem_PlayLoop(AudioSystem* system, AudioClipHandle handle, float volume);
void AudioSystem_StopClip(AudioSystem* system, AudioClipHandle handle);
void AudioSystem_StopAll(AudioSystem* system);

// State / controls
void AudioSystem_SetMasterVolume(AudioSystem* system, float volume);
float AudioSystem_GetMasterVolume(const AudioSystem* system);

void AudioSystem_SetClipVolume(AudioSystem* system, AudioClipHandle handle, float volume);
float AudioSystem_GetClipVolume(const AudioSystem* system, AudioClipHandle handle);

void AudioSystem_SetClipLooping(AudioSystem* system, AudioClipHandle handle, b32 should_loop);
b32 AudioSystem_IsClipPlaying(const AudioSystem* system, AudioClipHandle handle);
void AudioSystem_PauseClip(AudioSystem* system, AudioClipHandle handle);
void AudioSystem_ResumeClip(AudioSystem* system, AudioClipHandle handle);

// Debug
void AudioSystem_LogSummary(const AudioSystem* system);

#ifdef __cplusplus
}
#endif

#endif  // ENGINE_AUDIO_SYSTEM_H