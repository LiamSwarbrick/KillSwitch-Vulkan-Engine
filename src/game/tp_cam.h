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
    inline void TPCam_SyncFovFromRenderer(TPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

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

    inline EntityID TPCam_FindFirstBindablePlayer(ECS* ecs)
    {
        if (!ecs) return NULL_ENTITY;

        EntityID found = NULL_ENTITY;
        ecs->GetView<C_Transform, C_AnimatedMesh>().ForEach([&](EntityID id, C_Transform&, C_AnimatedMesh&)
        {
            if (found == NULL_ENTITY)
                found = id;
        });

        return found;
    }

    inline CameraInfo TPCam_Update(TPCamState& cam, ECS* ecs, float dt, bool allow_mouse_look, bool apply_fov_to_renderer)
    {
        static constexpr float MOUSE_SENSITIVITY  = 0.10f;
        static constexpr float GAMEPAD_LOOK_SPEED = 120.0f;

        TPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            TPCam_ApplyFovToRenderer(cam);

        if (allow_mouse_look)
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY;
        }

        cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
            * GAMEPAD_LOOK_SPEED * dt;
        cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
            * GAMEPAD_LOOK_SPEED * dt;

        if (cam.pitch > 80.0f) cam.pitch = 80.0f;
        if (cam.pitch < -80.0f) cam.pitch = -80.0f;
        if (cam.distance < 0.5f) cam.distance = 0.5f;

        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

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
