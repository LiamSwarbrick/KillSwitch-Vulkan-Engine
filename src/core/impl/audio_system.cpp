#include "core/audio_system.h"

#include "SDL3/SDL.h"

#if defined(_WIN32)
   #ifndef NOMINMAX
       #define NOMINMAX
   #endif
   #ifndef WIN32_LEAN_AND_MEAN
       #define WIN32_LEAN_AND_MEAN
   #endif
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

#if defined(_WIN32)
   #include <windows.h>
   #ifdef min
       #undef min
   #endif
   #ifdef max
       #undef max
   #endif
#endif

namespace
{
constexpr float kDefaultVolume = 1.0f;
constexpr float kDefaultMinDistance = 1.0f;
constexpr float kDefaultMaxDistance = 50.0f;
constexpr float kDefaultLowPassCutoff = 20000.0f;
constexpr ma_uint32 kListenerIndex = 0;
constexpr size_t kMaxActiveOneShotVoices = 64;

struct AudioClipRecord
{
   AudioClipHandle handle = 0;
   std::string logical_name;
   std::string source_path;
   std::string resolved_path;

   float clip_volume = kDefaultVolume;
   float playback_volume = kDefaultVolume;
   b32 looping = 0;
   b32 is_playing = 0;
   b32 paused = 0;
   ma_uint64 paused_cursor = 0;

   AudioClipCategory category = AUDIO_CLIP_CATEGORY_SFX;

   b32 spatialized = 0;
   float position_x = 0.0f;
   float position_y = 0.0f;
   float position_z = 0.0f;
   float min_distance = kDefaultMinDistance;
   float max_distance = kDefaultMaxDistance;

   b32 low_pass_enabled = 0;
   float low_pass_cutoff_hz = kDefaultLowPassCutoff;

   ma_sound sound = {};
   b32 sound_initialized = 0;
};

struct AudioOneShotVoice
{
   AudioClipHandle source_handle = 0;
   AudioClipCategory category = AUDIO_CLIP_CATEGORY_SFX;
   float clip_volume = kDefaultVolume;
   float playback_volume = kDefaultVolume;
   b32 spatialized = 0;
   float position_x = 0.0f;
   float position_y = 0.0f;
   float position_z = 0.0f;
   float min_distance = kDefaultMinDistance;
   float max_distance = kDefaultMaxDistance;
   ma_sound sound = {};
   b32 sound_initialized = 0;
};

struct AudioSystemImpl
{
   std::vector<std::unique_ptr<AudioClipRecord>> clips;
   std::vector<std::unique_ptr<AudioOneShotVoice>> one_shot_voices;
   ma_context context = {};
   b32 context_initialized = 0;
   ma_device device = {};
   b32 device_initialized = 0;
   ma_engine engine = {};
   b32 engine_initialized = 0;

   float master_volume = kDefaultVolume;
   float sfx_volume = kDefaultVolume;
   float ambient_volume = kDefaultVolume;
   float soundtrack_volume = kDefaultVolume;

   b32 ambient_enabled = 1;
   b32 soundtrack_enabled = 1;

   float listener_x = 0.0f;
   float listener_y = 0.0f;
   float listener_z = 0.0f;

   float listener_forward_x = 0.0f;
   float listener_forward_y = 0.0f;
   float listener_forward_z = -1.0f;

   float listener_up_x = 0.0f;
   float listener_up_y = 1.0f;
   float listener_up_z = 0.0f;

   AudioClipHandle current_soundtrack_handle = 0;
};

static AudioSystemImpl*
get_impl(AudioSystem* system)
{
   if (system == nullptr)
   {
       return nullptr;
   }

   return static_cast<AudioSystemImpl*>(system->impl);
}

static const AudioSystemImpl*
get_impl_const(const AudioSystem* system)
{
   return get_impl(const_cast<AudioSystem*>(system));
}

static AudioClipRecord*
get_clip_mut(AudioSystem* system, AudioClipHandle handle)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || handle == 0)
   {
       return nullptr;
   }

   const size_t index = static_cast<size_t>(handle - 1);
   if (index >= impl->clips.size())
   {
       return nullptr;
   }

   return impl->clips[index].get();
}

static const AudioClipRecord*
get_clip_const(const AudioSystem* system, AudioClipHandle handle)
{
   return get_clip_mut(const_cast<AudioSystem*>(system), handle);
}

static float
clamp_volume(float volume)
{
   return std::clamp(volume, 0.0f, 1.0f);
}

static bool
file_exists(const std::filesystem::path& path)
{
   std::error_code ec;
   return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

static std::filesystem::path
find_project_root(std::filesystem::path start)
{
   if (start.empty())
   {
       return {};
   }

   std::error_code ec;
   if (std::filesystem::is_regular_file(start, ec))
   {
       start = start.parent_path();
   }

   while (!start.empty())
   {
       if (file_exists(start / "premake5.lua"))
       {
           return start;
       }

       const std::filesystem::path parent = start.parent_path();
       if (parent == start)
       {
           break;
       }

       start = parent;
   }

   return {};
}

static std::filesystem::path
get_executable_directory()
{
#if defined(_WIN32)
   char buffer[MAX_PATH] = {};
   const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
   if (length == 0 || length == MAX_PATH)
   {
       return {};
   }

   return std::filesystem::path(buffer).parent_path();
#else
   return {};
#endif
}

static std::filesystem::path
get_compile_time_project_root()
{
   std::filesystem::path source_file = std::filesystem::path(__FILE__);
   if (!source_file.is_absolute())
   {
       std::error_code ec;
       const std::filesystem::path current_directory = std::filesystem::current_path(ec);
       if (!current_directory.empty())
       {
           source_file = (current_directory / source_file).lexically_normal();
       }
   }

   return find_project_root(source_file);
}

static std::string
resolve_audio_path(const char* source_path)
{
   if (source_path == nullptr || source_path[0] == '\0')
   {
       return {};
   }

   const std::filesystem::path requested_path(source_path);
   if (requested_path.is_absolute() && file_exists(requested_path))
   {
       return requested_path.lexically_normal().string();
   }

   std::vector<std::filesystem::path> roots;

   const std::filesystem::path compile_time_project_root = get_compile_time_project_root();
   if (!compile_time_project_root.empty())
   {
       roots.push_back(compile_time_project_root);
   }

   std::error_code ec;
   const std::filesystem::path current_directory = std::filesystem::current_path(ec);
   if (!current_directory.empty())
   {
       roots.push_back(current_directory);

       const std::filesystem::path project_root = find_project_root(current_directory);
       if (!project_root.empty() && project_root != current_directory)
       {
           roots.push_back(project_root);
       }
   }

   const std::filesystem::path executable_directory = get_executable_directory();
   if (!executable_directory.empty())
   {
       roots.push_back(executable_directory);

       const std::filesystem::path project_root = find_project_root(executable_directory);
       if (!project_root.empty() && project_root != executable_directory)
       {
           roots.push_back(project_root);
       }
   }

   for (const std::filesystem::path& root : roots)
   {
       const std::filesystem::path candidate = (root / requested_path).lexically_normal();
       if (file_exists(candidate))
       {
           return candidate.string();
       }
   }

   return {};
}

static void
ensure_sdl_audio_ready()
{
   if (SDL_WasInit(SDL_INIT_AUDIO) != 0)
   {
       return;
   }

   if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: SDL audio subsystem did not initialize: %s", SDL_GetError());
       return;
   }

   SDL_Log("AudioSystem: SDL audio subsystem initialized for playback.");
}

static const char*
audio_notification_name(ma_device_notification_type type)
{
   switch (type)
   {
       case ma_device_notification_type_started: return "started";
       case ma_device_notification_type_stopped: return "stopped";
       case ma_device_notification_type_rerouted: return "rerouted";
       case ma_device_notification_type_interruption_began: return "interruption_began";
       case ma_device_notification_type_interruption_ended: return "interruption_ended";
       case ma_device_notification_type_unlocked: return "unlocked";
       default: return "unknown";
   }
}

static void
audio_device_notification_callback(const ma_device_notification* notification)
{
   if (notification == nullptr || notification->pDevice == nullptr)
   {
       return;
   }

   char device_name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
   if (ma_device_get_name(notification->pDevice, ma_device_type_playback, device_name, sizeof(device_name), NULL) != MA_SUCCESS)
   {
       SDL_strlcpy(device_name, "<unknown>", sizeof(device_name));
   }

   SDL_Log(
       "AudioSystem: device notification=%s, playback_device=%s, state=%d",
       audio_notification_name(notification->type),
       device_name,
       (int)ma_device_get_state(notification->pDevice)
   );
}

static void
audio_device_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count)
{
   (void)input;

   if (device == nullptr || output == nullptr)
   {
       return;
   }

   AudioSystemImpl* impl = static_cast<AudioSystemImpl*>(device->pUserData);
   if (impl == nullptr || !impl->engine_initialized)
   {
       ma_silence_pcm_frames(output, frame_count, device->playback.format, device->playback.channels);
       return;
   }

   ma_uint64 frames_read = 0;
   const ma_result result = ma_engine_read_pcm_frames(&impl->engine, output, frame_count, &frames_read);
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: engine callback failed to read frames (%d).", (int)result);
       ma_silence_pcm_frames(output, frame_count, device->playback.format, device->playback.channels);
       return;
   }

   if (frames_read < frame_count)
   {
       ma_silence_pcm_frames(
           ma_offset_pcm_frames_ptr(output, frames_read, device->playback.format, device->playback.channels),
           frame_count - static_cast<ma_uint32>(frames_read),
           device->playback.format,
           device->playback.channels
       );
   }
}

static void
log_available_playback_devices(AudioSystemImpl* impl)
{
   if (impl == nullptr || !impl->context_initialized)
   {
       return;
   }

   ma_device_info* playback_infos = nullptr;
   ma_uint32 playback_count = 0;
   if (ma_context_get_devices(&impl->context, &playback_infos, &playback_count, NULL, NULL) != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to enumerate playback devices.");
       return;
   }

   SDL_Log("AudioSystem: detected %u playback device(s).", playback_count);
   for (ma_uint32 index = 0; index < playback_count; ++index)
   {
       SDL_Log("AudioSystem: playback_device[%u]=%s", index, playback_infos[index].name);
   }
}

static void
uninit_audio_device(AudioSystemImpl* impl)
{
   if (impl == nullptr || !impl->device_initialized)
   {
       return;
   }

   ma_device_uninit(&impl->device);
   impl->device = {};
   impl->device_initialized = 0;
}

static void
uninit_audio_context(AudioSystemImpl* impl)
{
   if (impl == nullptr || !impl->context_initialized)
   {
       return;
   }

   ma_context_uninit(&impl->context);
   impl->context = {};
   impl->context_initialized = 0;
}

static b32
init_audio_context(AudioSystemImpl* impl, const ma_backend* backends, ma_uint32 backend_count, const char* label)
{
   if (impl == nullptr)
   {
       return 0;
   }

   ma_context_config context_config = ma_context_config_init();
   const ma_result result = ma_context_init(backends, backend_count, &context_config, &impl->context);
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: context init failed for %s (%d).", label, (int)result);
       return 0;
   }

   impl->context_initialized = 1;
   return 1;
}

static b32
init_audio_device(AudioSystemImpl* impl, const char* label)
{
   if (impl == nullptr || !impl->context_initialized)
   {
       return 0;
   }

   ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
   device_config.playback.format = ma_format_f32;
   device_config.playback.channels = 2;
#if defined(_WIN32)
   device_config.playback.shareMode = ma_share_mode_shared;
#endif
    device_config.performanceProfile = ma_performance_profile_low_latency;
   device_config.dataCallback = audio_device_data_callback;
   device_config.notificationCallback = audio_device_notification_callback;
   device_config.pUserData = impl;
   device_config.noPreSilencedOutputBuffer = MA_TRUE;
   device_config.noClip = MA_TRUE;

   const ma_result result = ma_device_init(&impl->context, &device_config, &impl->device);
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: playback device init failed for %s (%d).", label, (int)result);
       return 0;
   }

   impl->device_initialized = 1;
   return 1;
}

static void
log_engine_device(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || !impl->engine_initialized || !impl->device_initialized)
   {
       return;
   }

   ma_device* device = ma_engine_get_device(&impl->engine);
   if (device == nullptr)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: engine has no playback device.");
       return;
   }

   char device_name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
   if (ma_device_get_name(device, ma_device_type_playback, device_name, sizeof(device_name), NULL) != MA_SUCCESS)
   {
       SDL_strlcpy(device_name, "<unknown>", sizeof(device_name));
   }

   const char* backend_name = device->pContext != nullptr ? ma_get_backend_name(device->pContext->backend) : "<unknown>";
   const ma_device_state state = ma_device_get_state(device);

   SDL_Log(
       "AudioSystem: backend=%s, playback_device=%s, sample_rate=%u, channels=%u, device_state=%d",
       backend_name,
       device_name,
       device->sampleRate,
       device->playback.channels,
       (int)state
   );
}

static float
get_category_volume(const AudioSystemImpl* impl, AudioClipCategory category)
{
   if (impl == nullptr)
   {
       return 1.0f;
   }

   switch (category)
   {
       case AUDIO_CLIP_CATEGORY_AMBIENT:
           return impl->ambient_volume;
       case AUDIO_CLIP_CATEGORY_SOUNDTRACK:
           return impl->soundtrack_volume;
       case AUDIO_CLIP_CATEGORY_SFX:
       default:
           return impl->sfx_volume;
   }
}

static b32
is_category_enabled(const AudioSystemImpl* impl, AudioClipCategory category)
{
   if (impl == nullptr)
   {
       return 0;
   }

   switch (category)
   {
       case AUDIO_CLIP_CATEGORY_AMBIENT:
           return impl->ambient_enabled;
       case AUDIO_CLIP_CATEGORY_SOUNDTRACK:
           return impl->soundtrack_enabled;
       case AUDIO_CLIP_CATEGORY_SFX:
       default:
           return 1;
   }
}

static float
compute_effective_volume(const AudioSystemImpl* impl, const AudioClipRecord* clip)
{
   if (impl == nullptr || clip == nullptr)
   {
       return 0.0f;
   }

   return clamp_volume(
       get_category_volume(impl, clip->category) *
       clip->clip_volume *
       clip->playback_volume
   );
}

static float
compute_effective_voice_volume(const AudioSystemImpl* impl, const AudioOneShotVoice* voice)
{
   if (impl == nullptr || voice == nullptr)
   {
       return 0.0f;
   }

   return clamp_volume(
       get_category_volume(impl, voice->category) *
       voice->clip_volume *
       voice->playback_volume
   );
}

static void
apply_master_volume(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || !impl->engine_initialized)
   {
       return;
   }

   (void)ma_engine_set_volume(&impl->engine, impl->master_volume);
}

static void
apply_clip_settings(AudioSystem* system, AudioClipRecord* clip)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || clip == nullptr || !clip->sound_initialized)
   {
       return;
   }

   ma_sound_set_volume(&clip->sound, compute_effective_volume(impl, clip));
   ma_sound_set_looping(&clip->sound, clip->looping ? MA_TRUE : MA_FALSE);
   ma_sound_set_spatialization_enabled(&clip->sound, clip->spatialized ? MA_TRUE : MA_FALSE);
   ma_sound_set_position(&clip->sound, clip->position_x, clip->position_y, clip->position_z);
   ma_sound_set_min_distance(&clip->sound, clip->min_distance);
   ma_sound_set_max_distance(&clip->sound, clip->max_distance);
}

static void
apply_voice_settings(AudioSystemImpl* impl, AudioOneShotVoice* voice)
{
   if (impl == nullptr || voice == nullptr || !voice->sound_initialized)
   {
       return;
   }

   ma_sound_set_volume(&voice->sound, compute_effective_voice_volume(impl, voice));
   ma_sound_set_looping(&voice->sound, MA_FALSE);
   ma_sound_set_spatialization_enabled(&voice->sound, voice->spatialized ? MA_TRUE : MA_FALSE);
   ma_sound_set_position(&voice->sound, voice->position_x, voice->position_y, voice->position_z);
   ma_sound_set_min_distance(&voice->sound, voice->min_distance);
   ma_sound_set_max_distance(&voice->sound, voice->max_distance);
}

static void
uninit_one_shot_voice(std::unique_ptr<AudioOneShotVoice>& voice)
{
   if (voice != nullptr && voice->sound_initialized)
   {
       (void)ma_sound_stop(&voice->sound);
       ma_sound_uninit(&voice->sound);
       voice->sound_initialized = 0;
   }
}

static void
cleanup_finished_one_shots(AudioSystemImpl* impl)
{
   if (impl == nullptr)
   {
       return;
   }

   impl->one_shot_voices.erase(
       std::remove_if(
           impl->one_shot_voices.begin(),
           impl->one_shot_voices.end(),
           [](std::unique_ptr<AudioOneShotVoice>& voice)
           {
               if (voice == nullptr || !voice->sound_initialized || !ma_sound_is_playing(&voice->sound))
               {
                   uninit_one_shot_voice(voice);
                   return true;
               }

               return false;
           }),
       impl->one_shot_voices.end()
   );
}

static void
refresh_clip_state(AudioSystem* system, AudioClipRecord* clip)
{
   (void)system;
   if (clip == nullptr || !clip->sound_initialized)
   {
       return;
   }

   if (clip->paused)
   {
       clip->is_playing = 1;
       return;
   }

   clip->is_playing = ma_sound_is_playing(&clip->sound) ? 1 : 0;
}

static void
refresh_all_live_volumes(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   apply_master_volume(system);

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       apply_clip_settings(system, clip.get());
   }

   for (const std::unique_ptr<AudioOneShotVoice>& voice : impl->one_shot_voices)
   {
       apply_voice_settings(impl, voice.get());
   }
}

static void
stop_clip_internal(AudioSystem* system, AudioClipRecord* clip)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || clip == nullptr || !clip->sound_initialized)
   {
       return;
   }

   (void)ma_sound_stop(&clip->sound);
   (void)ma_sound_seek_to_pcm_frame(&clip->sound, 0);
   ma_sound_reset_stop_time_and_fade(&clip->sound);

   clip->is_playing = 0;
   clip->paused = 0;
   clip->paused_cursor = 0;

   if (impl->current_soundtrack_handle == clip->handle)
   {
       impl->current_soundtrack_handle = 0;
   }

   impl->one_shot_voices.erase(
       std::remove_if(
           impl->one_shot_voices.begin(),
           impl->one_shot_voices.end(),
           [clip](std::unique_ptr<AudioOneShotVoice>& voice)
           {
               if (voice != nullptr && voice->source_handle == clip->handle)
               {
                   uninit_one_shot_voice(voice);
                   return true;
               }

               return false;
           }),
       impl->one_shot_voices.end()
   );
}

static b32
play_clip_internal(AudioSystem* system, AudioClipRecord* clip, float volume, b32 loop)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || clip == nullptr || !clip->sound_initialized)
   {
       return 0;
   }

   if (!is_category_enabled(impl, clip->category))
   {
       return 0;
   }

   if (clip->category == AUDIO_CLIP_CATEGORY_SOUNDTRACK &&
       impl->current_soundtrack_handle != 0 &&
       impl->current_soundtrack_handle != clip->handle)
   {
       AudioSystem_StopClip(system, impl->current_soundtrack_handle);
   }

   clip->playback_volume = clamp_volume(volume);
   clip->looping = loop ? 1 : 0;
   clip->paused = 0;
   clip->paused_cursor = 0;

   ma_sound_reset_stop_time_and_fade(&clip->sound);
   (void)ma_sound_stop(&clip->sound);
   (void)ma_sound_seek_to_pcm_frame(&clip->sound, 0);

   apply_clip_settings(system, clip);

   const ma_result result = ma_sound_start(&clip->sound);
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to start '%s' (miniaudio result %d).", clip->logical_name.c_str(), (int)result);
       clip->is_playing = 0;
       return 0;
   }

   SDL_Log("AudioSystem: started '%s' at volume %.2f (loop=%d).", clip->logical_name.c_str(), compute_effective_volume(impl, clip), loop ? 1 : 0);
   clip->is_playing = 1;

   if (clip->category == AUDIO_CLIP_CATEGORY_SOUNDTRACK)
   {
       impl->current_soundtrack_handle = clip->handle;
   }

   return 1;
}

static b32
play_one_shot_voice_internal(AudioSystem* system, AudioClipRecord* clip, float volume)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || clip == nullptr || !clip->sound_initialized)
   {
       return 0;
   }

   if (!is_category_enabled(impl, clip->category))
   {
       return 0;
   }

   cleanup_finished_one_shots(impl);
   if (impl->one_shot_voices.size() >= kMaxActiveOneShotVoices)
   {
       uninit_one_shot_voice(impl->one_shot_voices.front());
       impl->one_shot_voices.erase(impl->one_shot_voices.begin());
   }

   std::unique_ptr<AudioOneShotVoice> voice(new (std::nothrow) AudioOneShotVoice());
   if (!voice)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to allocate one-shot voice for '%s'.", clip->logical_name.c_str());
       return play_clip_internal(system, clip, volume, 0);
   }

   const ma_result copy_result = ma_sound_init_copy(&impl->engine, &clip->sound, 0, NULL, &voice->sound);
   if (copy_result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to copy one-shot '%s' (%d); falling back to shared clip.", clip->logical_name.c_str(), (int)copy_result);
       return play_clip_internal(system, clip, volume, 0);
   }

   voice->sound_initialized = 1;
   voice->source_handle = clip->handle;
   voice->category = clip->category;
   voice->clip_volume = clip->clip_volume;
   voice->playback_volume = clamp_volume(volume);
   voice->spatialized = clip->spatialized;
   voice->position_x = clip->position_x;
   voice->position_y = clip->position_y;
   voice->position_z = clip->position_z;
   voice->min_distance = clip->min_distance;
   voice->max_distance = clip->max_distance;

   ma_sound_reset_stop_time_and_fade(&voice->sound);
   (void)ma_sound_seek_to_pcm_frame(&voice->sound, 0);
   apply_voice_settings(impl, voice.get());

   const ma_result start_result = ma_sound_start(&voice->sound);
   if (start_result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to start one-shot '%s' (%d).", clip->logical_name.c_str(), (int)start_result);
       uninit_one_shot_voice(voice);
       return 0;
   }

   impl->one_shot_voices.push_back(std::move(voice));
   return 1;
}

static void
update_listener(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || !impl->engine_initialized)
   {
       return;
   }

   ma_engine_listener_set_position(
       &impl->engine,
       kListenerIndex,
       impl->listener_x, impl->listener_y, impl->listener_z
   );
   ma_engine_listener_set_direction(
       &impl->engine,
       kListenerIndex,
       impl->listener_forward_x, impl->listener_forward_y, impl->listener_forward_z
   );
   ma_engine_listener_set_world_up(
       &impl->engine,
       kListenerIndex,
       impl->listener_up_x, impl->listener_up_y, impl->listener_up_z
   );
}
}

AudioSystem
AudioSystem_Create(AudioSystemCreateInfo create_info)
{
   AudioSystem system = {};

   const char* tracker_name = create_info.debug_name;
   if (tracker_name == nullptr || tracker_name[0] == '\0')
   {
       tracker_name = "AudioSystem";
   }

   system.tt = init_per_thread_allocation_tracker(tracker_name);

   std::unique_ptr<AudioSystemImpl> impl(new (std::nothrow) AudioSystemImpl());
   if (!impl)
   {
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to allocate backend state.");
       return system;
   }

   const size_t reserve_count = create_info.initial_capacity > 0 ? create_info.initial_capacity : 8u;
   impl->clips.reserve(reserve_count);
   impl->master_volume = clamp_volume(create_info.master_volume > 0.0f ? create_info.master_volume : 1.0f);

   ensure_sdl_audio_ready();

#if defined(_WIN32)
   const struct AudioBackendAttempt
   {
       const char* label;
       ma_backend backend;
   }
   attempts[] = {
       { "WASAPI", ma_backend_wasapi },
       { "DirectSound", ma_backend_dsound },
       { "WinMM", ma_backend_winmm }
   };

   b32 initialized_backend = 0;
   for (const AudioBackendAttempt& attempt : attempts)
   {
       if (!init_audio_context(impl.get(), &attempt.backend, 1, attempt.label))
       {
           continue;
       }

       log_available_playback_devices(impl.get());
       if (init_audio_device(impl.get(), attempt.label))
       {
           SDL_Log("AudioSystem: initialized playback backend via %s.", attempt.label);
           initialized_backend = 1;
           break;
       }

       uninit_audio_context(impl.get());
   }

   if (!initialized_backend)
   {
       if (!init_audio_context(impl.get(), NULL, 0, "default backend search"))
       {
           SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize audio context.");
           return system;
       }

       log_available_playback_devices(impl.get());
       if (!init_audio_device(impl.get(), "default backend search"))
       {
           SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize playback device.");
           uninit_audio_context(impl.get());
           return system;
       }
   }
#else
   if (!init_audio_context(impl.get(), NULL, 0, "default backend search"))
   {
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize audio context.");
       return system;
   }

   log_available_playback_devices(impl.get());
   if (!init_audio_device(impl.get(), "default backend search"))
   {
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize playback device.");
       uninit_audio_context(impl.get());
       return system;
   }
#endif

   ma_engine_config engine_config = ma_engine_config_init();
   engine_config.pContext = &impl->context;
   engine_config.pDevice = &impl->device;
   const ma_result result = ma_engine_init(&engine_config, &impl->engine);
   if (result != MA_SUCCESS)
   {
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to initialize miniaudio engine (%d).", (int)result);
       uninit_audio_device(impl.get());
       uninit_audio_context(impl.get());
       return system;
   }

   impl->engine_initialized = 1;
   system.impl = impl.release();

   apply_master_volume(&system);
   update_listener(&system);

   const ma_result start_result = ma_engine_start(&get_impl(&system)->engine);
   if (start_result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to start miniaudio engine (%d).", (int)start_result);
   }

   log_engine_device(&system);
   return system;
}

void
AudioSystem_Destroy(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   AudioSystem_StopAll(system);

   for (std::unique_ptr<AudioOneShotVoice>& voice : impl->one_shot_voices)
   {
       uninit_one_shot_voice(voice);
   }
   impl->one_shot_voices.clear();

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       if (clip != nullptr && clip->sound_initialized)
       {
           ma_sound_uninit(&clip->sound);
       }
   }

   if (impl->engine_initialized)
   {
       ma_engine_uninit(&impl->engine);
       impl->engine_initialized = 0;
   }

   uninit_audio_device(impl);
   uninit_audio_context(impl);

   delete impl;
   system->impl = nullptr;
}

void
AudioSystem_Update(AudioSystem* system, float dt)
{
   (void)dt;

   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       refresh_clip_state(system, clip.get());
   }

   cleanup_finished_one_shots(impl);
}

AudioClipHandle
AudioSystem_FindClipByName(const AudioSystem* system, const char* logical_name)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   if (impl == nullptr || logical_name == nullptr || logical_name[0] == '\0')
   {
       return 0;
   }

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       if (clip->logical_name == logical_name)
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
   if (impl == nullptr || source_path == nullptr || source_path[0] == '\0')
   {
       return 0;
   }

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       if (clip->source_path == source_path || clip->resolved_path == source_path)
       {
           return clip->handle;
       }
   }

   return 0;
}

AudioClipHandle
AudioSystem_LoadClip(AudioSystem* system, const char* logical_name, const char* source_path)
{
   return AudioSystem_LoadClipEx(system, logical_name, source_path, AUDIO_CLIP_CATEGORY_SFX);
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
   if (impl == nullptr || !impl->engine_initialized || source_path == nullptr || source_path[0] == '\0')
   {
       return 0;
   }

   if (logical_name != nullptr && logical_name[0] != '\0')
   {
       const AudioClipHandle existing_handle = AudioSystem_FindClipByName(system, logical_name);
       if (existing_handle != 0)
       {
           return existing_handle;
       }
   }

   const AudioClipHandle existing_by_path = AudioSystem_FindClipByPath(system, source_path);
   if (existing_by_path != 0)
   {
       return existing_by_path;
   }

   const std::string resolved_path = resolve_audio_path(source_path);
   if (resolved_path.empty())
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: could not find audio file '%s'.", source_path);
       return 0;
   }

   std::unique_ptr<AudioClipRecord> clip(new (std::nothrow) AudioClipRecord());
   if (!clip)
   {
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to allocate clip state.");
       return 0;
   }

   clip->handle = static_cast<AudioClipHandle>(impl->clips.size() + 1);
   clip->logical_name = (logical_name != nullptr && logical_name[0] != '\0') ? logical_name : ("clip_" + std::to_string(clip->handle));
   clip->source_path = source_path;
   clip->resolved_path = resolved_path;
   clip->category = category;

   const ma_result result = ma_sound_init_from_file(
       &impl->engine,
       clip->resolved_path.c_str(),
       0,
       NULL,
       NULL,
       &clip->sound
   );
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to load '%s' with miniaudio (%d).", clip->resolved_path.c_str(), (int)result);
       return 0;
   }

   SDL_Log("AudioSystem: loaded '%s' from '%s'.", clip->logical_name.c_str(), clip->resolved_path.c_str());

   clip->sound_initialized = 1;
   apply_clip_settings(system, clip.get());

   impl->clips.push_back(std::move(clip));
   return impl->clips.back()->handle;
}

b32
AudioSystem_PlayOneShot(AudioSystem* system, AudioClipHandle handle, float volume)
{
   return play_one_shot_voice_internal(system, get_clip_mut(system, handle), volume);
}

b32
AudioSystem_PlayLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
   return play_clip_internal(system, get_clip_mut(system, handle), volume, 1);
}

void
AudioSystem_StopClip(AudioSystem* system, AudioClipHandle handle)
{
   stop_clip_internal(system, get_clip_mut(system, handle));
}

void
AudioSystem_StopAll(AudioSystem* system)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
   {
       stop_clip_internal(system, clip.get());
   }

   for (std::unique_ptr<AudioOneShotVoice>& voice : impl->one_shot_voices)
   {
       uninit_one_shot_voice(voice);
   }
   impl->one_shot_voices.clear();
}

b32
AudioSystem_PlaySFXOneShot(AudioSystem* system, AudioClipHandle handle, float volume)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return 0;
   }

   clip->category = AUDIO_CLIP_CATEGORY_SFX;
   return AudioSystem_PlayOneShot(system, handle, volume);
}

b32
AudioSystem_PlayAmbientLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return 0;
   }

   clip->category = AUDIO_CLIP_CATEGORY_AMBIENT;
   return AudioSystem_PlayLoop(system, handle, volume);
}

b32
AudioSystem_PlaySoundtrackLoop(AudioSystem* system, AudioClipHandle handle, float volume)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return 0;
   }

   clip->category = AUDIO_CLIP_CATEGORY_SOUNDTRACK;
   return AudioSystem_PlayLoop(system, handle, volume);
}

b32
AudioSystem_PlaySoundtrack(AudioSystem* system, AudioClipHandle handle, float volume)
{
   return AudioSystem_PlaySoundtrackLoop(system, handle, volume);
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
   if (clip == nullptr)
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
   return AudioSystem_PlayLoop(system, handle, volume);
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
   if (clip == nullptr)
   {
       return 0;
   }

   clip->category = AUDIO_CLIP_CATEGORY_SFX;
   clip->spatialized = 1;
   clip->position_x = x;
   clip->position_y = y;
   clip->position_z = z;
   clip->min_distance = min_distance;
   clip->max_distance = max_distance;
   return AudioSystem_PlayOneShot(system, handle, volume);
}

b32
AudioSystem_PlaySoundtrackFaded(AudioSystem* system, AudioClipHandle handle, float target_volume, float fade_seconds)
{
   AudioSystemImpl* impl = get_impl(system);
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (impl == nullptr || clip == nullptr || !clip->sound_initialized)
   {
       return 0;
   }

   clip->category = AUDIO_CLIP_CATEGORY_SOUNDTRACK;
   clip->playback_volume = clamp_volume(target_volume);
   clip->looping = 1;

   if (!is_category_enabled(impl, clip->category))
   {
       return 0;
   }

   if (impl->current_soundtrack_handle != 0 &&
       impl->current_soundtrack_handle != clip->handle)
   {
       AudioSystem_StopClip(system, impl->current_soundtrack_handle);
   }

   ma_sound_reset_stop_time_and_fade(&clip->sound);
   (void)ma_sound_stop(&clip->sound);
   (void)ma_sound_seek_to_pcm_frame(&clip->sound, 0);
   ma_sound_set_looping(&clip->sound, MA_TRUE);
   ma_sound_set_volume(&clip->sound, 0.0f);

   ma_sound_set_spatialization_enabled(&clip->sound, clip->spatialized ? MA_TRUE : MA_FALSE);
   ma_sound_set_position(&clip->sound, clip->position_x, clip->position_y, clip->position_z);
   ma_sound_set_min_distance(&clip->sound, clip->min_distance);
   ma_sound_set_max_distance(&clip->sound, clip->max_distance);

   if (fade_seconds > 0.0f)
   {
       const ma_uint64 fade_frames = static_cast<ma_uint64>(ma_engine_get_sample_rate(&impl->engine) * fade_seconds);
       ma_sound_set_fade_in_pcm_frames(&clip->sound, 0.0f, compute_effective_volume(impl, clip), fade_frames);
   }
   else
   {
       ma_sound_set_volume(&clip->sound, compute_effective_volume(impl, clip));
   }

   const ma_result result = ma_sound_start(&clip->sound);
   if (result != MA_SUCCESS)
   {
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "AudioSystem: failed to start faded soundtrack '%s' (%d).", clip->logical_name.c_str(), (int)result);
       clip->is_playing = 0;
       return 0;
   }

   clip->paused = 0;
   clip->paused_cursor = 0;
   clip->is_playing = 1;
   impl->current_soundtrack_handle = clip->handle;
   return 1;
}

void
AudioSystem_StopSoundtrackFaded(AudioSystem* system, float fade_seconds)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr || impl->current_soundtrack_handle == 0)
   {
       return;
   }

   AudioClipRecord* clip = get_clip_mut(system, impl->current_soundtrack_handle);
   if (clip == nullptr || !clip->sound_initialized)
   {
       impl->current_soundtrack_handle = 0;
       return;
   }

   if (fade_seconds > 0.0f)
   {
       (void)ma_sound_stop_with_fade_in_milliseconds(&clip->sound, static_cast<ma_uint64>(fade_seconds * 1000.0f));
       clip->is_playing = 0;
       clip->paused = 0;
       impl->current_soundtrack_handle = 0;
       return;
   }

   AudioSystem_StopClip(system, clip->handle);
}

void
AudioSystem_SetAmbientEnabled(AudioSystem* system, b32 enabled)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   impl->ambient_enabled = enabled ? 1 : 0;
   if (!impl->ambient_enabled)
   {
       for (const std::unique_ptr<AudioClipRecord>& clip : impl->clips)
       {
           if (clip->category == AUDIO_CLIP_CATEGORY_AMBIENT)
           {
               stop_clip_internal(system, clip.get());
           }
       }
   }
}

b32
AudioSystem_GetAmbientEnabled(const AudioSystem* system)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   return impl != nullptr ? impl->ambient_enabled : 0;
}

void
AudioSystem_SetSoundtrackEnabled(AudioSystem* system, b32 enabled)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   impl->soundtrack_enabled = enabled ? 1 : 0;
   if (!impl->soundtrack_enabled && impl->current_soundtrack_handle != 0)
   {
       AudioSystem_StopClip(system, impl->current_soundtrack_handle);
   }
}

b32
AudioSystem_GetSoundtrackEnabled(const AudioSystem* system)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   return impl != nullptr ? impl->soundtrack_enabled : 0;
}

void
AudioSystem_SetMasterVolume(AudioSystem* system, float volume)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   impl->master_volume = clamp_volume(volume);
   refresh_all_live_volumes(system);
}

float
AudioSystem_GetMasterVolume(const AudioSystem* system)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   return impl != nullptr ? impl->master_volume : 0.0f;
}

void
AudioSystem_SetCategoryVolume(AudioSystem* system, AudioClipCategory category, float volume)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   switch (category)
   {
       case AUDIO_CLIP_CATEGORY_AMBIENT:
           impl->ambient_volume = clamp_volume(volume);
           break;
       case AUDIO_CLIP_CATEGORY_SOUNDTRACK:
           impl->soundtrack_volume = clamp_volume(volume);
           break;
       case AUDIO_CLIP_CATEGORY_SFX:
       default:
           impl->sfx_volume = clamp_volume(volume);
           break;
   }

   refresh_all_live_volumes(system);
}

float
AudioSystem_GetCategoryVolume(const AudioSystem* system, AudioClipCategory category)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   return get_category_volume(impl, category);
}

void
AudioSystem_SetClipVolume(AudioSystem* system, AudioClipHandle handle, float volume)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return;
   }

   clip->clip_volume = clamp_volume(volume);
   apply_clip_settings(system, clip);
}

float
AudioSystem_GetClipVolume(const AudioSystem* system, AudioClipHandle handle)
{
   const AudioClipRecord* clip = get_clip_const(system, handle);
   return clip != nullptr ? clip->clip_volume : 0.0f;
}

void
AudioSystem_SetClipLooping(AudioSystem* system, AudioClipHandle handle, b32 should_loop)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return;
   }

   clip->looping = should_loop ? 1 : 0;
   apply_clip_settings(system, clip);
}

b32
AudioSystem_IsClipPlaying(const AudioSystem* system, AudioClipHandle handle)
{
   AudioClipRecord* clip = get_clip_mut(const_cast<AudioSystem*>(system), handle);
   if (clip == nullptr)
   {
       return 0;
   }

   refresh_clip_state(const_cast<AudioSystem*>(system), clip);
   if (clip->is_playing)
   {
       return 1;
   }

   const AudioSystemImpl* impl = get_impl_const(system);
   if (impl == nullptr)
   {
       return 0;
   }

   for (const std::unique_ptr<AudioOneShotVoice>& voice : impl->one_shot_voices)
   {
       if (voice != nullptr &&
           voice->source_handle == handle &&
           voice->sound_initialized &&
           ma_sound_is_playing(&voice->sound))
       {
           return 1;
       }
   }

   return 0;
}

void
AudioSystem_PauseClip(AudioSystem* system, AudioClipHandle handle)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr || !clip->sound_initialized || clip->paused)
   {
       return;
   }

   ma_uint64 cursor = 0;
   (void)ma_sound_get_cursor_in_pcm_frames(&clip->sound, &cursor);
   clip->paused_cursor = cursor;
   clip->paused = 1;
   clip->is_playing = 1;
   (void)ma_sound_stop(&clip->sound);
}

void
AudioSystem_ResumeClip(AudioSystem* system, AudioClipHandle handle)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr || !clip->sound_initialized || !clip->paused)
   {
       return;
   }

   ma_sound_reset_stop_time_and_fade(&clip->sound);
   (void)ma_sound_seek_to_pcm_frame(&clip->sound, clip->paused_cursor);
   if (ma_sound_start(&clip->sound) == MA_SUCCESS)
   {
       clip->paused = 0;
       clip->is_playing = 1;
   }
}

void
AudioSystem_SetListenerPosition(AudioSystem* system, float x, float y, float z)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   impl->listener_x = x;
   impl->listener_y = y;
   impl->listener_z = z;
   update_listener(system);
}

void
AudioSystem_SetListenerOrientation(
   AudioSystem* system,
   float forward_x, float forward_y, float forward_z,
   float up_x, float up_y, float up_z
)
{
   AudioSystemImpl* impl = get_impl(system);
   if (impl == nullptr)
   {
       return;
   }

   impl->listener_forward_x = forward_x;
   impl->listener_forward_y = forward_y;
   impl->listener_forward_z = forward_z;
   impl->listener_up_x = up_x;
   impl->listener_up_y = up_y;
   impl->listener_up_z = up_z;
   update_listener(system);
}

void
AudioSystem_SetClipSpatialized(AudioSystem* system, AudioClipHandle handle, b32 enabled)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip != nullptr)
   {
       clip->spatialized = enabled ? 1 : 0;
       apply_clip_settings(system, clip);
   }
}

void
AudioSystem_SetClipPosition(AudioSystem* system, AudioClipHandle handle, float x, float y, float z)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return;
   }

   clip->position_x = x;
   clip->position_y = y;
   clip->position_z = z;
   apply_clip_settings(system, clip);
}

void
AudioSystem_SetClipMinMaxDistance(AudioSystem* system, AudioClipHandle handle, float min_distance, float max_distance)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip == nullptr)
   {
       return;
   }

   clip->min_distance = std::max(0.0f, min_distance);
   clip->max_distance = std::max(clip->min_distance, max_distance);
   apply_clip_settings(system, clip);
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
   AudioSystem_SetListenerPosition(system, listener_x, listener_y, listener_z);
   AudioSystem_SetListenerOrientation(system, forward_x, forward_y, forward_z, up_x, up_y, up_z);

   if (clip_states == nullptr)
   {
       return;
   }

   for (u32 index = 0; index < clip_state_count; ++index)
   {
       AudioSystem_SetClipPosition(
           system,
           clip_states[index].handle,
           clip_states[index].position_x,
           clip_states[index].position_y,
           clip_states[index].position_z
       );
   }
}

b32
AudioSystem_PlaySpatialOneShot(
   AudioSystem* system,
   AudioClipHandle handle,
   float volume,
   float x, float y, float z
)
{
   AudioSystem_SetClipSpatialized(system, handle, 1);
   AudioSystem_SetClipPosition(system, handle, x, y, z);
   return AudioSystem_PlayOneShot(system, handle, volume);
}

b32
AudioSystem_PlaySpatialLoop(
   AudioSystem* system,
   AudioClipHandle handle,
   float volume,
   float x, float y, float z
)
{
   AudioSystem_SetClipSpatialized(system, handle, 1);
   AudioSystem_SetClipPosition(system, handle, x, y, z);
   return AudioSystem_PlayLoop(system, handle, volume);
}

void
AudioSystem_SetClipLowPassEnabled(AudioSystem* system, AudioClipHandle handle, b32 enabled)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip != nullptr)
   {
       clip->low_pass_enabled = enabled ? 1 : 0;
   }
}

void
AudioSystem_SetClipLowPassCutoff(AudioSystem* system, AudioClipHandle handle, float cutoff_hz)
{
   AudioClipRecord* clip = get_clip_mut(system, handle);
   if (clip != nullptr)
   {
       clip->low_pass_cutoff_hz = std::max(20.0f, cutoff_hz);
   }
}

void
AudioSystem_LogSummary(const AudioSystem* system)
{
   const AudioSystemImpl* impl = get_impl_const(system);
   if (impl == nullptr)
   {
       SDL_Log("AudioSystem: not initialized.");
       return;
   }

   SDL_Log(
       "AudioSystem: clips=%u, master=%.2f, soundtrack_enabled=%d, ambient_enabled=%d, engine_initialized=%d",
       static_cast<unsigned>(impl->clips.size()),
       impl->master_volume,
       impl->soundtrack_enabled,
       impl->ambient_enabled,
       impl->engine_initialized
   );
}
