#include "core/audio_system.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <assert.h>

typedef struct AudioClipRecord
{
    AudioClipHandle handle;
    b32 valid;
    u32 generation;

    char logical_name[AUDIO_NAME_MAX_LEN];
    char source_path[AUDIO_PATH_MAX_LEN];

    ma_sound* sound;
    float volume;
    b32 looping;

    AudioClipCategory category;

    // Spatial
    b32 spatialized;
    float position_x;
    float position_y;
    float position_z;
    float min_distance;
    float max_distance;

    // Low-pass placeholder state
    b32 low_pass_enabled;
    float low_pass_cutoff_hz;
}
AudioClipRecord;

typedef struct AudioSystemImpl
{
    ma_engine engine;
    b32 engine_initialized;

    AudioClipRecord* clips;
    u32 clip_count;
    u32 clip_capacity;

    float master_volume;
    float sfx_volume;
    float ambient_volume;
    float soundtrack_volume;

    float listener_x;
    float listener_y;
    float listener_z;
}
AudioSystemImpl;

static inline AudioSystemImpl*
get_impl(AudioSystem* system)
{
    if (system == NULL) return NULL;
    return (AudioSystemImpl*)system->impl;
}

static inline const AudioSystemImpl*
get_impl_const(const AudioSystem* system)
{
    if (system == NULL) return NULL;
    return (const AudioSystemImpl*)system->impl;
}

static inline AudioClipRecord*
get_clip_mut(AudioSystem* system, AudioClipHandle handle)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || handle == 0) return NULL;

    u32 index = handle - 1;
    if (index >= impl->clip_count) return NULL;

    return &impl->clips[index];
}

static inline const AudioClipRecord*
get_clip_const(const AudioSystem* system, AudioClipHandle handle)
{
    return get_clip_mut((AudioSystem*)system, handle);
}

static void
copy_string_safe(char* dst, size_t dst_size, const char* src, const char* fallback)
{
    const char* s = src;
    if (s == NULL || s[0] == '\0')
    {
        s = fallback;
    }

    strncpy(dst, s, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void
ensure_capacity_for_one_more(AudioSystem* system)
{
    AudioSystemImpl* impl = get_impl(system);
    SDL_assert(impl != NULL);

    if (impl->clip_count < impl->clip_capacity)
    {
        return;
    }

    u32 old_capacity = impl->clip_capacity;
    u32 new_capacity = (old_capacity == 0) ? 16 : old_capacity * 2;

    AudioClipRecord* new_clips = (AudioClipRecord*)L_realloc(
        impl->clips,
        sizeof(AudioClipRecord) * new_capacity,
        &system->tt
    );

    memset(
        new_clips + old_capacity,
        0,
        sizeof(AudioClipRecord) * (new_capacity - old_capacity)
    );

    impl->clips = new_clips;
    impl->clip_capacity = new_capacity;
}

static float
get_category_volume(const AudioSystemImpl* impl, AudioClipCategory category)
{
    if (impl == NULL) return 1.0f;

    switch (category)
    {
        case AUDIO_CLIP_CATEGORY_AMBIENT:    return impl->ambient_volume;
        case AUDIO_CLIP_CATEGORY_SOUNDTRACK: return impl->soundtrack_volume;
        case AUDIO_CLIP_CATEGORY_SFX:
        default:                             return impl->sfx_volume;
    }
}

static void
apply_clip_runtime_state(AudioSystem* system, AudioClipRecord* clip)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || clip == NULL || clip->sound == NULL || !impl->engine_initialized)
    {
        return;
    }

    float final_volume = clip->volume * get_category_volume(impl, clip->category);

    ma_sound_set_volume(clip->sound, final_volume);
    ma_sound_set_looping(clip->sound, clip->looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(clip->sound, clip->spatialized ? MA_TRUE : MA_FALSE);

    if (clip->spatialized)
    {
        ma_sound_set_position(clip->sound, clip->position_x, clip->position_y, clip->position_z);
        ma_sound_set_min_distance(clip->sound, clip->min_distance);
        ma_sound_set_max_distance(clip->sound, clip->max_distance);
        ma_sound_set_attenuation_model(clip->sound, ma_attenuation_model_inverse);
    }

    // NOTE:
    // low_pass_enabled / low_pass_cutoff_hz are stored here for future DSP integration.
    // In this submission version we intentionally do not pretend they are wired into a
    // true miniaudio effect graph yet.
}

AudioSystem
AudioSystem_Create(AudioSystemCreateInfo create_info)
{
    AudioSystem system = {};

    const char* tracker_name = create_info.debug_name;
    if (tracker_name == NULL || tracker_name[0] == '\0')
    {
        tracker_name = "AudioSystem";
    }

    system.tt = init_per_thread_allocation_tracker(tracker_name);

    AudioSystemImpl* impl = (AudioSystemImpl*)L_calloc(1, sizeof(AudioSystemImpl), &system.tt);

    impl->clip_capacity = (create_info.initial_capacity == 0) ? 16 : create_info.initial_capacity;
    impl->clips = (AudioClipRecord*)L_calloc(impl->clip_capacity, sizeof(AudioClipRecord), &system.tt);

    impl->master_volume = (create_info.master_volume <= 0.0f) ? 1.0f : create_info.master_volume;
    impl->sfx_volume = 1.0f;
    impl->ambient_volume = 1.0f;
    impl->soundtrack_volume = 1.0f;

    impl->listener_x = 0.0f;
    impl->listener_y = 0.0f;
    impl->listener_z = 0.0f;

    ma_engine_config config = ma_engine_config_init();
    if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize miniaudio engine");
        impl->engine_initialized = 0;
    }
    else
    {
        impl->engine_initialized = 1;
        ma_engine_set_volume(&impl->engine, impl->master_volume);
        ma_engine_listener_set_position(&impl->engine, 0, impl->listener_x, impl->listener_y, impl->listener_z);
        SDL_Log("AudioSystem: initialized");
    }

    system.impl = impl;
    return system;
}

void
AudioSystem_Destroy(AudioSystem* system)
{
    if (system == NULL || system->impl == NULL)
    {
        return;
    }

    AudioSystemImpl* impl = get_impl(system);

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        AudioClipRecord* clip = &impl->clips[i];
        if (clip->sound != NULL)
        {
            ma_sound_uninit(clip->sound);
            L_free(clip->sound, &system->tt);
            clip->sound = NULL;
        }
    }

    if (impl->engine_initialized)
    {
        ma_engine_uninit(&impl->engine);
        impl->engine_initialized = 0;
    }

    if (impl->clips != NULL)
    {
        L_free(impl->clips, &system->tt);
        impl->clips = NULL;
    }

    L_free(impl, &system->tt);
    system->impl = NULL;

    check_tracker_for_memory_leaks(&system->tt);
}

AudioClipHandle
AudioSystem_FindClipByName(const AudioSystem* system, const char* logical_name)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL || logical_name == NULL || logical_name[0] == '\0')
    {
        return 0;
    }

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        const AudioClipRecord* clip = &impl->clips[i];
        if (clip->valid && strncmp(clip->logical_name, logical_name, AUDIO_NAME_MAX_LEN) == 0)
        {
            return clip->handle;
        }
    }

    return 0;
}

AudioClipHandle
AudioSystem_FindClipByPath(const AudioSystem* system, const char* source_path)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL || source_path == NULL || source_path[0] == '\0')
    {
        return 0;
    }

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        const AudioClipRecord* clip = &impl->clips[i];
        if (clip->valid && strncmp(clip->source_path, source_path, AUDIO_PATH_MAX_LEN) == 0)
        {
            return clip->handle;
        }
    }

    return 0;
}

AudioClipHandle
AudioSystem_LoadClip(AudioSystem* system, const char* logical_name, const char* source_path)
{
    return AudioSystem_LoadClipEx(
        system,
        logical_name,
        source_path,
        AUDIO_CLIP_CATEGORY_SFX
    );
}

AudioClipHandle
AudioSystem_LoadClipEx(
    AudioSystem* system,
    const char* logical_name,
    const char* source_path,
    AudioClipCategory category
)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || !impl->engine_initialized || source_path == NULL || source_path[0] == '\0')
    {
        return 0;
    }

    AudioClipHandle existing = 0;
    if (logical_name && logical_name[0] != '\0')
    {
        existing = AudioSystem_FindClipByName(system, logical_name);
    }

    if (existing == 0)
    {
        existing = AudioSystem_FindClipByPath(system, source_path);
    }

    if (existing != 0)
    {
        return existing;
    }

    ensure_capacity_for_one_more(system);

    AudioClipRecord* clip = &impl->clips[impl->clip_count];
    memset(clip, 0, sizeof(*clip));

    clip->handle = impl->clip_count + 1;
    clip->valid = 1;
    clip->generation = 0;
    clip->volume = 1.0f;
    clip->looping = 0;
    clip->category = category;

    clip->spatialized = 0;
    clip->position_x = 0.0f;
    clip->position_y = 0.0f;
    clip->position_z = 0.0f;
    clip->min_distance = 1.0f;
    clip->max_distance = 50.0f;

    clip->low_pass_enabled = 0;
    clip->low_pass_cutoff_hz = 20000.0f;

    copy_string_safe(clip->logical_name, sizeof(clip->logical_name), logical_name, source_path);
    copy_string_safe(clip->source_path, sizeof(clip->source_path), source_path, "<missing-path>");

    clip->sound = (ma_sound*)L_calloc(1, sizeof(ma_sound), &system->tt);

    ma_result result = ma_sound_init_from_file(
        &impl->engine,
        clip->source_path,
        0,
        NULL,
        NULL,
        clip->sound
    );

    if (result != MA_SUCCESS)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "AudioSystem: failed to load clip '%s' from '%s'",
            clip->logical_name,
            clip->source_path
        );

        L_free(clip->sound, &system->tt);
        clip->sound = NULL;
        clip->valid = 0;
        return 0;
    }

    apply_clip_runtime_state(system, clip);

    ++clip->generation;
    ++impl->clip_count;

    SDL_Log(
        "AudioSystem: loaded clip [%u] '%s' from '%s'",
        clip->handle,
        clip->logical_name,
        clip->source_path
    );

    return clip->handle;
}

b32
AudioSystem_PlayOneShot(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return 0;
    }

    clip->looping = 0;
    clip->volume = volume;
    clip->spatialized = 0;

    apply_clip_runtime_state(system, clip);

    ma_sound_stop(clip->sound);
    ma_sound_seek_to_pcm_frame(clip->sound, 0);

    return ma_sound_start(clip->sound) == MA_SUCCESS;
}

b32
AudioSystem_PlayLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return 0;
    }

    clip->looping = 1;
    clip->volume = volume;
    clip->spatialized = 0;

    apply_clip_runtime_state(system, clip);

    ma_sound_stop(clip->sound);
    ma_sound_seek_to_pcm_frame(clip->sound, 0);

    return ma_sound_start(clip->sound) == MA_SUCCESS;
}

b32
AudioSystem_PlaySFXOneShot(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL) return 0;
    clip->category = AUDIO_CLIP_CATEGORY_SFX;
    return AudioSystem_PlayOneShot(system, handle, volume);
}

b32
AudioSystem_PlayAmbientLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL) return 0;
    clip->category = AUDIO_CLIP_CATEGORY_AMBIENT;
    return AudioSystem_PlayLoop(system, handle, volume);
}

b32
AudioSystem_PlaySoundtrackLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL) return 0;
    clip->category = AUDIO_CLIP_CATEGORY_SOUNDTRACK;
    return AudioSystem_PlayLoop(system, handle, volume);
}

b32
AudioSystem_PlaySpatialOneShot(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z
)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return 0;
    }

    clip->looping = 0;
    clip->volume = volume;
    clip->spatialized = 1;
    clip->position_x = x;
    clip->position_y = y;
    clip->position_z = z;

    apply_clip_runtime_state(system, clip);

    ma_sound_stop(clip->sound);
    ma_sound_seek_to_pcm_frame(clip->sound, 0);

    return ma_sound_start(clip->sound) == MA_SUCCESS;
}

b32
AudioSystem_PlaySpatialLoop(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z
)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return 0;
    }

    clip->looping = 1;
    clip->volume = volume;
    clip->spatialized = 1;
    clip->position_x = x;
    clip->position_y = y;
    clip->position_z = z;

    apply_clip_runtime_state(system, clip);

    ma_sound_stop(clip->sound);
    ma_sound_seek_to_pcm_frame(clip->sound, 0);

    return ma_sound_start(clip->sound) == MA_SUCCESS;
}

void
AudioSystem_StopClip(AudioSystem* system, AudioClipHandle handle)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    ma_sound_stop(clip->sound);
}

void
AudioSystem_StopAll(AudioSystem* system)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL)
    {
        return;
    }

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        AudioClipRecord* clip = &impl->clips[i];
        if (clip->valid && clip->sound != NULL)
        {
            ma_sound_stop(clip->sound);
        }
    }
}

void
AudioSystem_SetMasterVolume(AudioSystem* system, float volume)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || !impl->engine_initialized)
    {
        return;
    }

    impl->master_volume = volume;
    ma_engine_set_volume(&impl->engine, volume);
}

float
AudioSystem_GetMasterVolume(const AudioSystem* system)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL)
    {
        return 0.0f;
    }

    return impl->master_volume;
}

void
AudioSystem_SetCategoryVolume(AudioSystem* system, AudioClipCategory category, float volume)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL)
    {
        return;
    }

    switch (category)
    {
        case AUDIO_CLIP_CATEGORY_AMBIENT:    impl->ambient_volume = volume; break;
        case AUDIO_CLIP_CATEGORY_SOUNDTRACK: impl->soundtrack_volume = volume; break;
        case AUDIO_CLIP_CATEGORY_SFX:
        default:                             impl->sfx_volume = volume; break;
    }

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        AudioClipRecord* clip = &impl->clips[i];
        if (clip->valid && clip->category == category)
        {
            apply_clip_runtime_state(system, clip);
        }
    }
}

float
AudioSystem_GetCategoryVolume(const AudioSystem* system, AudioClipCategory category)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL)
    {
        return 0.0f;
    }

    return get_category_volume(impl, category);
}

void
AudioSystem_SetClipVolume(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    clip->volume = volume;
    apply_clip_runtime_state(system, clip);
}

float
AudioSystem_GetClipVolume(const AudioSystem* system, AudioClipHandle handle)
{
    const AudioClipRecord* clip = get_clip_const(system, handle);
    if (clip == NULL)
    {
        return 0.0f;
    }

    return clip->volume;
}

void
AudioSystem_SetClipLooping(AudioSystem* system, AudioClipHandle handle, b32 should_loop)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    clip->looping = should_loop ? 1 : 0;
    ma_sound_set_looping(clip->sound, should_loop ? MA_TRUE : MA_FALSE);
}

b32
AudioSystem_IsClipPlaying(const AudioSystem* system, AudioClipHandle handle)
{
    const AudioClipRecord* clip = get_clip_const(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return 0;
    }

    return ma_sound_is_playing(clip->sound);
}

void
AudioSystem_PauseClip(AudioSystem* system, AudioClipHandle handle)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    ma_sound_stop(clip->sound);
}

void
AudioSystem_ResumeClip(AudioSystem* system, AudioClipHandle handle)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    ma_sound_start(clip->sound);
}

void
AudioSystem_SetListenerPosition(AudioSystem* system, float x, float y, float z)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || !impl->engine_initialized)
    {
        return;
    }

    impl->listener_x = x;
    impl->listener_y = y;
    impl->listener_z = z;

    ma_engine_listener_set_position(&impl->engine, 0, x, y, z);
}

void
AudioSystem_SetClipSpatialized(AudioSystem* system, AudioClipHandle handle, b32 enabled)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    clip->spatialized = enabled ? 1 : 0;
    apply_clip_runtime_state(system, clip);
}

void
AudioSystem_SetClipPosition(AudioSystem* system, AudioClipHandle handle, float x, float y, float z)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    clip->position_x = x;
    clip->position_y = y;
    clip->position_z = z;

    if (clip->spatialized)
    {
        ma_sound_set_position(clip->sound, x, y, z);
    }
}

void
AudioSystem_SetClipMinMaxDistance(AudioSystem* system, AudioClipHandle handle, float min_distance, float max_distance)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    clip->min_distance = min_distance;
    clip->max_distance = max_distance;

    if (clip->spatialized)
    {
        ma_sound_set_min_distance(clip->sound, min_distance);
        ma_sound_set_max_distance(clip->sound, max_distance);
    }
}

void
AudioSystem_SetClipLowPassEnabled(AudioSystem* system, AudioClipHandle handle, b32 enabled)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL)
    {
        return;
    }

    clip->low_pass_enabled = enabled ? 1 : 0;
}

void
AudioSystem_SetClipLowPassCutoff(AudioSystem* system, AudioClipHandle handle, float cutoff_hz)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL)
    {
        return;
    }

    clip->low_pass_cutoff_hz = cutoff_hz;
}

void
AudioSystem_LogSummary(const AudioSystem* system)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL)
    {
        return;
    }

    SDL_Log("--------------- AudioSystem Summary ---------------");
    SDL_Log(
        "clips=%u capacity=%u master=%.2f sfx=%.2f ambient=%.2f soundtrack=%.2f initialized=%d listener=(%.2f, %.2f, %.2f)",
        impl->clip_count,
        impl->clip_capacity,
        impl->master_volume,
        impl->sfx_volume,
        impl->ambient_volume,
        impl->soundtrack_volume,
        impl->engine_initialized,
        impl->listener_x,
        impl->listener_y,
        impl->listener_z
    );

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        const AudioClipRecord* clip = &impl->clips[i];
        SDL_Log(
            "[%u] name='%s' path='%s' gen=%u cat=%d vol=%.2f looping=%d spatial=%d pos=(%.2f, %.2f, %.2f) min=%.2f max=%.2f lpf=%d cutoff=%.2f",
            clip->handle,
            clip->logical_name,
            clip->source_path,
            clip->generation,
            (int)clip->category,
            clip->volume,
            clip->looping,
            clip->spatialized,
            clip->position_x,
            clip->position_y,
            clip->position_z,
            clip->min_distance,
            clip->max_distance,
            clip->low_pass_enabled,
            clip->low_pass_cutoff_hz
        );
    }

    SDL_Log("--------------------------------------------------");
}