#pragma once

#include "renderer/debug_ui_api.h"
#include "renderer/renderer.h"
#include "core/components.h"
#include "core/ecs.h"
#include "core/input.h"
#include "core/input_actions.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <cmath>

namespace Game
{
    // Pull renderer FOV once so TP camera state starts synchronized with render settings.
    inline void TPCam_SyncFovFromRenderer(TPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

    // Write TP camera FOV back to renderer when this camera is the active gameplay camera.
    inline void TPCam_ApplyFovToRenderer(const TPCamState& cam)
    {
        if (!cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        float target_fov_rad = glm::radians(cam.fov_deg);
        if (fabsf(settings.fov_y - target_fov_rad) > 0.0001f)
        {
            settings.fov_y = target_fov_rad;
            Renderer_ChangeSettings(settings);
        }
    }

    // Fallback bind target: first animated entity is treated as a player candidate.
    inline EntityID TPCam_FindFirstBindablePlayer(ECS* ecs)
    {
        if (!ecs) return NULL_ENTITY;

        EntityID found = NULL_ENTITY;
        ecs->GetView<C_PlayerInput>().ForEach([&](EntityID id, C_PlayerInput&)
        {
            if (found == NULL_ENTITY)
                found = id;
        });

        return found;
    }

    // Update TP camera orbit around target and return per-frame camera data.
    inline CameraInfo TPCam_Update(TPCamState& cam, ECS* ecs, float dt, bool allow_mouse_look, bool apply_fov_to_renderer)
    {
        static constexpr float MOUSE_SENSITIVITY  = 0.10f;
        static constexpr float GAMEPAD_LOOK_SPEED = 120.0f;

        // Keep FOV state and renderer projection synchronized.
        TPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            TPCam_ApplyFovToRenderer(cam);

        // Mouse look can be gated by caller (e.g. debug UI interaction).
        if (allow_mouse_look)
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY;
        }

        // Right-stick look remains active for controller orbit input.
        cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
            * GAMEPAD_LOOK_SPEED * dt;
        cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
            * GAMEPAD_LOOK_SPEED * dt;

        // Clamp vertical orbit and minimum distance for stable third-person framing.
        if (cam.pitch > 80.0f) cam.pitch = 80.0f;
        if (cam.pitch < -80.0f) cam.pitch = -80.0f;
        if (cam.distance < 0.5f) cam.distance = 0.5f;

        // Recompute orbit forward from yaw/pitch.
        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

        // Re-resolve bind target if the current entity is invalid or missing.
        C_Transform* bound_transform = nullptr;
        if (ecs && cam.bound_entity != NULL_ENTITY)
            bound_transform = ecs->GetComponentPtr<C_Transform>(cam.bound_entity);

        if (!bound_transform)
        {
            EntityID first_player = TPCam_FindFirstBindablePlayer(ecs);
            if (first_player != NULL_ENTITY)
            {
                cam.bound_entity = first_player;
                bound_transform = ecs->GetComponentPtr<C_Transform>(cam.bound_entity);
            }
        }

        // When bound, orbit around player head-height target.
        if (bound_transform)
        {
            glm::vec3 player_pos = glm::vec3(bound_transform->matrix[3]);
            cam.target = player_pos + glm::vec3(0.0f, cam.target_height, 0.0f);
        }

        // Keep TP orbit functional even when no entity is bound.
        cam.pos = cam.target - cam.forward * cam.distance;

        return CameraInfo{
            .view            = glm::lookAt(cam.pos, cam.target, glm::vec3(0.0f, 1.0f, 0.0f)),
            .position        = cam.pos,
            .lens_distortion = 0.0f
        };
    }
}
