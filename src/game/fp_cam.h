#pragma once

#include "renderer/debug_ui_api.h"
#include "renderer/renderer.h"
#include "core/components.h"
#include "foundations/components.h"
#include "core/ecs.h"
#include "core/input.h"
#include "core/input_actions.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <cmath>

namespace Game
{
    // Pull renderer FOV once so game/debug camera state starts synchronized.
    inline void FPCam_SyncFovFromRenderer(FPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

    inline void FPCam_ApplyFovToRenderer(const FPCamState& cam)
    {
        if (!cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        float target_fov_rad = glm::radians(cam.fov_deg);
        // Avoid redundant renderer setting updates when FOV is effectively unchanged.
        if (fabsf(settings.fov_y - target_fov_rad) > 0.0001f)
        {
            settings.fov_y = target_fov_rad;
            Renderer_ChangeSettings(settings);
        }
    }

    // Fallback binding target: first animated entity is treated as a player candidate.
    inline EntityID FPCam_FindFirstBindablePlayer(ECS* ecs)
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

    inline CameraInfo FPCam_Update(FPCamState& cam, ECS* ecs, float dt, bool allow_mouse_look, bool apply_fov_to_renderer)
    {
        static constexpr float MOUSE_SENSITIVITY  = 0.10f;
        static constexpr float GAMEPAD_LOOK_SPEED = 120.0f;

        // Keep FOV state and renderer projection synchronized.
        FPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            FPCam_ApplyFovToRenderer(cam);

        // Mouse look can be gated by caller (e.g. while interacting with debug UI).
        if (allow_mouse_look)
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY;
        }

        // Gamepad look remains active for controller-driven camera rotation.
        cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
            * GAMEPAD_LOOK_SPEED * dt;
        cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
            * GAMEPAD_LOOK_SPEED * dt;

        // Clamp pitch to prevent flip when approaching vertical poles.
        if (cam.pitch > 89.0f) cam.pitch = 89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        // Recompute forward vector from yaw/pitch.
        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

        // Re-resolve a bind target if the currently bound entity is invalid/missing.
        C_Transform* bound_transform = nullptr;
        if (ecs && cam.bound_entity != NULL_ENTITY)
            bound_transform = ecs->GetComponentPtr<C_Transform>(cam.bound_entity);

        if (!bound_transform)
        {
            EntityID first_player = FPCam_FindFirstBindablePlayer(ecs);
            if (first_player != NULL_ENTITY)
            {
                cam.bound_entity = first_player;
                bound_transform = ecs->GetComponentPtr<C_Transform>(cam.bound_entity);
            }
        }

        // Attach camera to bound entity position plus eye-height offset.
        if (bound_transform)
        {
            glm::vec3 player_pos = glm::vec3(bound_transform->matrix[3]);
            cam.pos = player_pos + glm::vec3(0.0f, cam.eye_height, 0.0f);
        }

        // Return per-frame camera data consumed by renderer.
        return CameraInfo{
            .view            = glm::lookAt(cam.pos, cam.pos + cam.forward, glm::vec3(0.0f, 1.0f, 0.0f)),
            .position        = cam.pos,
            .lens_distortion = 0.0f
        };
    }
}
