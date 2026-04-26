#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "core/core.h"
#include "SDL3/SDL.h"
#include "render_types.h"
#include "core/components.h"

// SETTINGS API

typedef struct Renderer_Settings
{
    b32         uncapped_fps;
    uint32_t    msaa_sample_count;
    float       fov_y;  // Radians 
}
Renderer_Settings;

typedef struct Renderer_SettingsCapabilities
{
    b32         supports_uncapped_fps;
    uint32_t    max_msaa_samples;
}
Renderer_SettingsCapabilities;

Renderer_SettingsCapabilities Renderer_GetSettingsCapabilities();
Renderer_Settings Renderer_GetSettings();
void Renderer_ChangeSettings(Renderer_Settings new_settings);


// INIT/SHUTDOWN/EVENTS API

typedef struct Renderer_InitInfo
{
    SDL_Window* window;
    bool enable_validation;
    Renderer_Settings preferred_initial_settings;
}
Renderer_InitInfo;

void Renderer_Init(const Renderer_InitInfo* info);
void Renderer_Shutdown();
void Renderer_ListenToWindowEvent(SDL_Event event);


// DRAW API

typedef struct CameraInfo
{
    glm::mat4 view;
    // glm::mat4 proj;  // <- Taken from fov setting
    glm::vec3 position;
    float lens_distortion;
}
CameraInfo;

void Renderer_PushRenderable(Renderable renderable);
void Renderer_PushLight(C_Light light, glm::vec3 position, glm::vec3 direction);

void Renderer_DrawFrame(CameraInfo main_camera);


// SCENE API (On scene change resources for GPU)

typedef struct Scene_InitInfo
{
    uint32_t num_static_meshes;
    C_StaticMesh** static_meshes;

    uint32_t num_animated_meshes;
    C_AnimatedMesh** animated_meshes;
}
Scene_InitInfo;
void Renderer_ChangeScene(Scene_InitInfo new_scene_info);

#endif  // ENGINE_RENDERER_H
