#include "core/audio_system.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <assert.h>

#define AUDIO_SFX_VOICE_POOL_SIZE 6

typedef struct AudioPlaybackVoice
{
    ma_sound* sound;
    ma_lpf_node* low_pass_node;
    b32 valid;
}
AudioPlaybackVoice;

typedef struct AudioClipRecord
{
    AudioClipHandle handle;
    b32 valid;
    u32 generation;

    char logical_name[AUDIO_NAME_MAX_LEN];
    char source_path[AUDIO_PATH_MAX_LEN];

    // Base sound path for looping / persistent usage.
    ma_sound* sound;
    ma_lpf_node* low_pass_node;

    // One-shot overlapping pool for SFX.
    AudioPlaybackVoice* one_shot_voices;
    u32 one_shot_voice_count;
    u32 next_one_shot_voice;

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

    // Low-pass
    b32 low_pass_enabled;
    float low_pass_cutoff_hz;
    u32 low_pass_order;
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

    b32 ambient_enabled;
    b32 soundtrack_enabled;

    float listener_x;
    float listener_y;
    float listener_z;

    float listener_forward_x;
    float listener_forward_y;
    float listener_forward_z;

    float listener_up_x;
    float listener_up_y;
    float listener_up_z;

    AudioClipHandle current_soundtrack_handle;

    // Soundtrack fade state
    b32 soundtrack_fade_active;
    b32 soundtrack_stop_after_fade;
    float soundtrack_fade_elapsed;
    float soundtrack_fade_duration;
    float soundtrack_fade_start_volume;
    float soundtrack_fade_target_volume;
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

static b32
is_category_enabled(const AudioSystemImpl* impl, AudioClipCategory category)
{
    if (impl == NULL) return 1;

    switch (category)
    {
        case AUDIO_CLIP_CATEGORY_AMBIENT:    return impl->ambient_enabled;
        case AUDIO_CLIP_CATEGORY_SOUNDTRACK: return impl->soundtrack_enabled;
        case AUDIO_CLIP_CATEGORY_SFX:
        default:                             return 1;
    }
}

static double
get_effective_low_pass_cutoff(const AudioSystemImpl* impl, const AudioClipRecord* clip)
{
    double sample_rate;
    double max_cutoff;
    double cutoff;

    if (impl == NULL || clip == NULL)
    {
        return 20000.0;
    }

    sample_rate = (double)ma_engine_get_sample_rate((ma_engine*)&impl->engine);
    max_cutoff = sample_rate * 0.49;

    if (!clip->low_pass_enabled)
    {
        return max_cutoff;
    }

    cutoff = (double)clip->low_pass_cutoff_hz;

    if (cutoff < 20.0)
    {
        cutoff = 20.0;
    }

    if (cutoff > max_cutoff)
    {
        cutoff = max_cutoff;
    }

    return cutoff;
}

static void
apply_low_pass_state_to_node(AudioSystem* system, AudioClipRecord* clip, ma_lpf_node* node)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || clip == NULL || node == NULL || !impl->engine_initialized)
    {
        return;
    }

    ma_uint32 channels = ma_engine_get_channels(&impl->engine);
    ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);

    ma_lpf_config lpfConfig = ma_lpf_config_init(
        ma_format_f32,
        channels,
        sampleRate,
        get_effective_low_pass_cutoff(impl, clip),
        clip->low_pass_order
    );

    ma_lpf_node_reinit(&lpfConfig, node);
}

static float
compute_final_volume(const AudioSystemImpl* impl, const AudioClipRecord* clip, float raw_volume)
{
    float final_volume = raw_volume * get_category_volume(impl, clip->category);

    if (!is_category_enabled(impl, clip->category))
    {
        final_volume = 0.0f;
    }

    return final_volume;
}

static void
apply_state_to_sound_instance(
    AudioSystem* system,
    AudioClipRecord* clip,
    ma_sound* sound,
    ma_lpf_node* low_pass_node,
    b32 looping,
    b32 spatialized,
    float x, float y, float z,
    float raw_volume
)
{
    AudioSystemImpl* impl = get_impl(system);
    float final_volume;

    if (impl == NULL || clip == NULL || sound == NULL || !impl->engine_initialized)
    {
        return;
    }

    final_volume = compute_final_volume(impl, clip, raw_volume);

    ma_sound_set_volume(sound, final_volume);
    ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(sound, spatialized ? MA_TRUE : MA_FALSE);

    if (spatialized)
    {
        ma_sound_set_position(sound, x, y, z);
        ma_sound_set_min_distance(sound, clip->min_distance);
        ma_sound_set_max_distance(sound, clip->max_distance);
        ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);
    }

    apply_low_pass_state_to_node(system, clip, low_pass_node);
}

static ma_result
create_sound_chain_from_file(
    AudioSystem* system,
    const char* source_path,
    ma_sound** out_sound,
    ma_lpf_node** out_low_pass_node
)
{
    AudioSystemImpl* impl = get_impl(system);
    ma_result result;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_lpf_node_config lpfNodeConfig;

    if (impl == NULL || out_sound == NULL || out_low_pass_node == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *out_sound = NULL;
    *out_low_pass_node = NULL;

    *out_sound = (ma_sound*)L_calloc(1, sizeof(ma_sound), &system->tt);
    *out_low_pass_node = (ma_lpf_node*)L_calloc(1, sizeof(ma_lpf_node), &system->tt);

    result = ma_sound_init_from_file(
        &impl->engine,
        source_path,
        MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT,
        NULL,
        NULL,
        *out_sound
    );

    if (result != MA_SUCCESS)
    {
        L_free(*out_sound, &system->tt);
        L_free(*out_low_pass_node, &system->tt);
        *out_sound = NULL;
        *out_low_pass_node = NULL;
        return result;
    }

    channels = ma_engine_get_channels(&impl->engine);
    sampleRate = ma_engine_get_sample_rate(&impl->engine);

    lpfNodeConfig = ma_lpf_node_config_init(
        channels,
        sampleRate,
        sampleRate * 0.49,
        2
    );

    result = ma_lpf_node_init(
        ma_engine_get_node_graph(&impl->engine),
        &lpfNodeConfig,
        NULL,
        *out_low_pass_node
    );

    if (result != MA_SUCCESS)
    {
        ma_sound_uninit(*out_sound);
        L_free(*out_sound, &system->tt);
        L_free(*out_low_pass_node, &system->tt);
        *out_sound = NULL;
        *out_low_pass_node = NULL;
        return result;
    }

    result = ma_node_attach_output_bus(
        *out_low_pass_node,
        0,
        ma_engine_get_endpoint(&impl->engine),
        0
    );

    if (result != MA_SUCCESS)
    {
        ma_lpf_node_uninit(*out_low_pass_node, NULL);
        ma_sound_uninit(*out_sound);
        L_free(*out_low_pass_node, &system->tt);
        L_free(*out_sound, &system->tt);
        *out_low_pass_node = NULL;
        *out_sound = NULL;
        return result;
    }

    result = ma_node_attach_output_bus(
        *out_sound,
        0,
        *out_low_pass_node,
        0
    );

    if (result != MA_SUCCESS)
    {
        ma_lpf_node_uninit(*out_low_pass_node, NULL);
        ma_sound_uninit(*out_sound);
        L_free(*out_low_pass_node, &system->tt);
        L_free(*out_sound, &system->tt);
        *out_low_pass_node = NULL;
        *out_sound = NULL;
        return result;
    }

    return MA_SUCCESS;
}

static void
destroy_sound_chain(AudioSystem* system, ma_sound** sound, ma_lpf_node** low_pass_node)
{
    if (sound != NULL && *sound != NULL)
    {
        ma_sound_uninit(*sound);
        L_free(*sound, &system->tt);
        *sound = NULL;
    }

    if (low_pass_node != NULL && *low_pass_node != NULL)
    {
        ma_lpf_node_uninit(*low_pass_node, NULL);
        L_free(*low_pass_node, &system->tt);
        *low_pass_node = NULL;
    }
}

static void
apply_clip_runtime_state(AudioSystem* system, AudioClipRecord* clip)
{
    if (clip == NULL || clip->sound == NULL)
    {
        return;
    }

    apply_state_to_sound_instance(
        system,
        clip,
        clip->sound,
        clip->low_pass_node,
        clip->looping,
        clip->spatialized,
        clip->position_x,
        clip->position_y,
        clip->position_z,
        clip->volume
    );

    if (clip->one_shot_voices != NULL)
    {
        for (u32 i = 0; i < clip->one_shot_voice_count; ++i)
        {
            AudioPlaybackVoice* voice = &clip->one_shot_voices[i];
            if (voice->valid && voice->sound != NULL)
            {
                apply_low_pass_state_to_node(system, clip, voice->low_pass_node);
            }
        }
    }
}

static AudioPlaybackVoice*
get_next_voice(AudioClipRecord* clip)
{
    if (clip == NULL || clip->one_shot_voices == NULL || clip->one_shot_voice_count == 0)
    {
        return NULL;
    }

    AudioPlaybackVoice* voice = &clip->one_shot_voices[clip->next_one_shot_voice];
    clip->next_one_shot_voice = (clip->next_one_shot_voice + 1) % clip->one_shot_voice_count;
    return voice;
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

    impl->ambient_enabled = 1;
    impl->soundtrack_enabled = 1;

    impl->listener_x = 0.0f;
    impl->listener_y = 0.0f;
    impl->listener_z = 0.0f;

    impl->listener_forward_x = 0.0f;
    impl->listener_forward_y = 0.0f;
    impl->listener_forward_z = -1.0f;

    impl->listener_up_x = 0.0f;
    impl->listener_up_y = 1.0f;
    impl->listener_up_z = 0.0f;

    impl->current_soundtrack_handle = 0;
    impl->soundtrack_fade_active = 0;
    impl->soundtrack_stop_after_fade = 0;

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
        ma_engine_listener_set_direction(&impl->engine, 0, impl->listener_forward_x, impl->listener_forward_y, impl->listener_forward_z);
        ma_engine_listener_set_world_up(&impl->engine, 0, impl->listener_up_x, impl->listener_up_y, impl->listener_up_z);
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

        destroy_sound_chain(system, &clip->sound, &clip->low_pass_node);

        if (clip->one_shot_voices != NULL)
        {
            for (u32 v = 0; v < clip->one_shot_voice_count; ++v)
            {
                AudioPlaybackVoice* voice = &clip->one_shot_voices[v];
                destroy_sound_chain(system, &voice->sound, &voice->low_pass_node);
                voice->valid = 0;
            }

            L_free(clip->one_shot_voices, &system->tt);
            clip->one_shot_voices = NULL;
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

void
AudioSystem_Update(AudioSystem* system, float dt)
{
    AudioSystemImpl* impl = get_impl(system);
    AudioClipRecord* soundtrack_clip;
    float t;
    float new_volume;

    if (impl == NULL || !impl->soundtrack_fade_active)
    {
        return;
    }

    if (impl->current_soundtrack_handle == 0)
    {
        impl->soundtrack_fade_active = 0;
        return;
    }

    soundtrack_clip = get_clip_mut(system, impl->current_soundtrack_handle);
    if (soundtrack_clip == NULL)
    {
        impl->soundtrack_fade_active = 0;
        return;
    }

    impl->soundtrack_fade_elapsed += dt;

    if (impl->soundtrack_fade_duration <= 0.0f)
    {
        t = 1.0f;
    }
    else
    {
        t = impl->soundtrack_fade_elapsed / impl->soundtrack_fade_duration;
        if (t > 1.0f) t = 1.0f;
    }

    new_volume = impl->soundtrack_fade_start_volume +
        (impl->soundtrack_fade_target_volume - impl->soundtrack_fade_start_volume) * t;

    soundtrack_clip->volume = new_volume;
    apply_clip_runtime_state(system, soundtrack_clip);

    if (t >= 1.0f)
    {
        impl->soundtrack_fade_active = 0;

        if (impl->soundtrack_stop_after_fade)
        {
            AudioSystem_StopClip(system, impl->current_soundtrack_handle);
            impl->current_soundtrack_handle = 0;
            impl->soundtrack_stop_after_fade = 0;
        }
    }
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
    AudioClipHandle existing = 0;
    AudioClipRecord* clip;
    ma_result result;

    if (impl == NULL || !impl->engine_initialized || source_path == NULL || source_path[0] == '\0')
    {
        return 0;
    }

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

    clip = &impl->clips[impl->clip_count];
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
    clip->low_pass_cutoff_hz = 1200.0f;
    clip->low_pass_order = 2;

    copy_string_safe(clip->logical_name, sizeof(clip->logical_name), logical_name, source_path);
    copy_string_safe(clip->source_path, sizeof(clip->source_path), source_path, "<missing-path>");

    result = create_sound_chain_from_file(system, clip->source_path, &clip->sound, &clip->low_pass_node);
    if (result != MA_SUCCESS)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "AudioSystem: failed to load clip '%s' from '%s'",
            clip->logical_name,
            clip->source_path
        );

        clip->valid = 0;
        return 0;
    }

    if (category == AUDIO_CLIP_CATEGORY_SFX)
    {
        clip->one_shot_voice_count = AUDIO_SFX_VOICE_POOL_SIZE;
        clip->one_shot_voices = (AudioPlaybackVoice*)L_calloc(
            clip->one_shot_voice_count,
            sizeof(AudioPlaybackVoice),
            &system->tt
        );

        for (u32 i = 0; i < clip->one_shot_voice_count; ++i)
        {
            AudioPlaybackVoice* voice = &clip->one_shot_voices[i];
            result = create_sound_chain_from_file(system, clip->source_path, &voice->sound, &voice->low_pass_node);

            if (result != MA_SUCCESS)
            {
                SDL_LogWarn(
                    SDL_LOG_CATEGORY_APPLICATION,
                    "AudioSystem: could not create SFX voice %u for '%s'",
                    i,
                    clip->logical_name
                );
                voice->valid = 0;
            }
            else
            {
                voice->valid = 1;
            }
        }
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
    AudioPlaybackVoice* voice;

    if (clip == NULL)
    {
        return 0;
    }

    if (clip->category == AUDIO_CLIP_CATEGORY_SFX && clip->one_shot_voices != NULL)
    {
        voice = get_next_voice(clip);
        if (voice == NULL || !voice->valid || voice->sound == NULL)
        {
            return 0;
        }

        apply_state_to_sound_instance(
            system,
            clip,
            voice->sound,
            voice->low_pass_node,
            0,
            0,
            0.0f, 0.0f, 0.0f,
            volume
        );

        ma_sound_stop(voice->sound);
        ma_sound_seek_to_pcm_frame(voice->sound, 0);
        return ma_sound_start(voice->sound) == MA_SUCCESS;
    }

    if (clip->sound == NULL)
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
AudioSystem_PlaySoundtrack(AudioSystem* system, AudioClipHandle handle, float volume)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL)
    {
        return 0;
    }

    if (impl->current_soundtrack_handle != 0 && impl->current_soundtrack_handle != handle)
    {
        AudioSystem_StopClip(system, impl->current_soundtrack_handle);
    }

    impl->current_soundtrack_handle = handle;
    impl->soundtrack_fade_active = 0;
    impl->soundtrack_stop_after_fade = 0;

    return AudioSystem_PlaySoundtrackLoop(system, handle, volume);
}

b32
AudioSystem_PlaySoundtrackFaded(AudioSystem* system, AudioClipHandle handle, float target_volume, float fade_seconds)
{
    AudioSystemImpl* impl = get_impl(system);
    AudioClipRecord* clip;

    if (impl == NULL)
    {
        return 0;
    }

    if (fade_seconds <= 0.0f)
    {
        return AudioSystem_PlaySoundtrack(system, handle, target_volume);
    }

    if (impl->current_soundtrack_handle != 0 && impl->current_soundtrack_handle != handle)
    {
        AudioSystem_StopClip(system, impl->current_soundtrack_handle);
    }

    impl->current_soundtrack_handle = handle;

    clip = get_clip_mut(system, handle);
    if (clip == NULL)
    {
        return 0;
    }

    clip->category = AUDIO_CLIP_CATEGORY_SOUNDTRACK;
    clip->volume = 0.0f;

    if (!AudioSystem_PlaySoundtrackLoop(system, handle, 0.0f))
    {
        return 0;
    }

    impl->soundtrack_fade_active = 1;
    impl->soundtrack_stop_after_fade = 0;
    impl->soundtrack_fade_elapsed = 0.0f;
    impl->soundtrack_fade_duration = fade_seconds;
    impl->soundtrack_fade_start_volume = 0.0f;
    impl->soundtrack_fade_target_volume = target_volume;

    return 1;
}

void
AudioSystem_StopSoundtrackFaded(AudioSystem* system, float fade_seconds)
{
    AudioSystemImpl* impl = get_impl(system);
    AudioClipRecord* clip;

    if (impl == NULL || impl->current_soundtrack_handle == 0)
    {
        return;
    }

    if (fade_seconds <= 0.0f)
    {
        AudioSystem_StopClip(system, impl->current_soundtrack_handle);
        impl->current_soundtrack_handle = 0;
        impl->soundtrack_fade_active = 0;
        impl->soundtrack_stop_after_fade = 0;
        return;
    }

    clip = get_clip_mut(system, impl->current_soundtrack_handle);
    if (clip == NULL)
    {
        impl->current_soundtrack_handle = 0;
        impl->soundtrack_fade_active = 0;
        impl->soundtrack_stop_after_fade = 0;
        return;
    }

    impl->soundtrack_fade_active = 1;
    impl->soundtrack_stop_after_fade = 1;
    impl->soundtrack_fade_elapsed = 0.0f;
    impl->soundtrack_fade_duration = fade_seconds;
    impl->soundtrack_fade_start_volume = clip->volume;
    impl->soundtrack_fade_target_volume = 0.0f;
}

b32
AudioSystem_PlayAmbientAt(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z,
    float min_distance,
    float max_distance
)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL)
    {
        return 0;
    }

    clip->category = AUDIO_CLIP_CATEGORY_AMBIENT;
    clip->spatialized = 1;
    clip->position_x = x;
    clip->position_y = y;
    clip->position_z = z;
    clip->min_distance = min_distance;
    clip->max_distance = max_distance;

    return AudioSystem_PlaySpatialLoop(system, handle, volume, x, y, z);
}

b32
AudioSystem_PlayEntitySFXAt(
    AudioSystem* system,
    AudioClipHandle handle,
    float volume,
    float x, float y, float z,
    float min_distance,
    float max_distance
)
{
    AudioClipRecord* clip = get_clip_mut(system, handle);
    if (clip == NULL)
    {
        return 0;
    }

    clip->category = AUDIO_CLIP_CATEGORY_SFX;
    clip->min_distance = min_distance;
    clip->max_distance = max_distance;

    return AudioSystem_PlaySpatialOneShot(system, handle, volume, x, y, z);
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
    AudioPlaybackVoice* voice;

    if (clip == NULL)
    {
        return 0;
    }

    if (clip->category == AUDIO_CLIP_CATEGORY_SFX && clip->one_shot_voices != NULL)
    {
        voice = get_next_voice(clip);
        if (voice == NULL || !voice->valid || voice->sound == NULL)
        {
            return 0;
        }

        apply_state_to_sound_instance(
            system,
            clip,
            voice->sound,
            voice->low_pass_node,
            0,
            1,
            x, y, z,
            volume
        );

        ma_sound_stop(voice->sound);
        ma_sound_seek_to_pcm_frame(voice->sound, 0);
        return ma_sound_start(voice->sound) == MA_SUCCESS;
    }

    if (clip->sound == NULL)
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
    if (clip == NULL)
    {
        return;
    }

    if (clip->sound != NULL)
    {
        ma_sound_stop(clip->sound);
    }

    if (clip->one_shot_voices != NULL)
    {
        for (u32 i = 0; i < clip->one_shot_voice_count; ++i)
        {
            AudioPlaybackVoice* voice = &clip->one_shot_voices[i];
            if (voice->valid && voice->sound != NULL)
            {
                ma_sound_stop(voice->sound);
            }
        }
    }
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
        AudioSystem_StopClip(system, impl->clips[i].handle);
    }

    impl->current_soundtrack_handle = 0;
}

void
AudioSystem_SetAmbientEnabled(AudioSystem* system, b32 enabled)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL)
    {
        return;
    }

    impl->ambient_enabled = enabled ? 1 : 0;

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        AudioClipRecord* clip = &impl->clips[i];
        if (clip->valid && clip->category == AUDIO_CLIP_CATEGORY_AMBIENT)
        {
            apply_clip_runtime_state(system, clip);
        }
    }
}

b32
AudioSystem_GetAmbientEnabled(const AudioSystem* system)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL) return 0;
    return impl->ambient_enabled;
}

void
AudioSystem_SetSoundtrackEnabled(AudioSystem* system, b32 enabled)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL)
    {
        return;
    }

    impl->soundtrack_enabled = enabled ? 1 : 0;

    if (impl->current_soundtrack_handle != 0)
    {
        AudioClipRecord* clip = get_clip_mut(system, impl->current_soundtrack_handle);
        if (clip != NULL)
        {
            apply_clip_runtime_state(system, clip);
        }
    }
}

b32
AudioSystem_GetSoundtrackEnabled(const AudioSystem* system)
{
    const AudioSystemImpl* impl = get_impl_const(system);
    if (impl == NULL) return 0;
    return impl->soundtrack_enabled;
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
    if (clip == NULL)
    {
        return;
    }

    if (clip->sound != NULL)
    {
        ma_sound_stop(clip->sound);
    }
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
AudioSystem_SetListenerOrientation(
    AudioSystem* system,
    float forward_x, float forward_y, float forward_z,
    float up_x, float up_y, float up_z
)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || !impl->engine_initialized)
    {
        return;
    }

    impl->listener_forward_x = forward_x;
    impl->listener_forward_y = forward_y;
    impl->listener_forward_z = forward_z;

    impl->listener_up_x = up_x;
    impl->listener_up_y = up_y;
    impl->listener_up_z = up_z;

    ma_engine_listener_set_direction(&impl->engine, 0, forward_x, forward_y, forward_z);
    ma_engine_listener_set_world_up(&impl->engine, 0, up_x, up_y, up_z);
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
AudioSystem_UpdateSpatialState(
    AudioSystem* system,
    float listener_x, float listener_y, float listener_z,
    float forward_x, float forward_y, float forward_z,
    float up_x, float up_y, float up_z,
    const AudioSpatialState* clip_states,
    u32 clip_state_count
)
{
    AudioSystemImpl* impl = get_impl(system);
    if (impl == NULL || !impl->engine_initialized)
    {
        return;
    }

    AudioSystem_SetListenerPosition(system, listener_x, listener_y, listener_z);
    AudioSystem_SetListenerOrientation(system, forward_x, forward_y, forward_z, up_x, up_y, up_z);

    if (clip_states == NULL || clip_state_count == 0)
    {
        return;
    }

    for (u32 i = 0; i < clip_state_count; ++i)
    {
        AudioClipRecord* clip = get_clip_mut(system, clip_states[i].handle);
        if (clip == NULL || clip->sound == NULL || !clip->spatialized)
        {
            continue;
        }

        clip->position_x = clip_states[i].position_x;
        clip->position_y = clip_states[i].position_y;
        clip->position_z = clip_states[i].position_z;

        ma_sound_set_position(
            clip->sound,
            clip->position_x,
            clip->position_y,
            clip->position_z
        );
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
    apply_clip_runtime_state(system, clip);
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
    apply_clip_runtime_state(system, clip);
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
        "clips=%u capacity=%u master=%.2f sfx=%.2f ambient=%.2f soundtrack=%.2f ambient_enabled=%d soundtrack_enabled=%d current_soundtrack=%u initialized=%d listener_pos=(%.2f, %.2f, %.2f) listener_fwd=(%.2f, %.2f, %.2f) listener_up=(%.2f, %.2f, %.2f)",
        impl->clip_count,
        impl->clip_capacity,
        impl->master_volume,
        impl->sfx_volume,
        impl->ambient_volume,
        impl->soundtrack_volume,
        impl->ambient_enabled,
        impl->soundtrack_enabled,
        impl->current_soundtrack_handle,
        impl->engine_initialized,
        impl->listener_x,
        impl->listener_y,
        impl->listener_z,
        impl->listener_forward_x,
        impl->listener_forward_y,
        impl->listener_forward_z,
        impl->listener_up_x,
        impl->listener_up_y,
        impl->listener_up_z
    );

    for (u32 i = 0; i < impl->clip_count; ++i)
    {
        const AudioClipRecord* clip = &impl->clips[i];
        SDL_Log(
            "[%u] name='%s' path='%s' gen=%u cat=%d vol=%.2f looping=%d spatial=%d pos=(%.2f, %.2f, %.2f) min=%.2f max=%.2f lpf=%d cutoff=%.2f order=%u one_shot_voices=%u",
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
            clip->low_pass_cutoff_hz,
            clip->low_pass_order,
            clip->one_shot_voice_count
        );
    }

    SDL_Log("--------------------------------------------------");
}