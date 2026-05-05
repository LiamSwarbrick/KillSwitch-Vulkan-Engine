#include "ingame_cam.h"

#include "core/components.h"
#include "core/input.h"
#include "core/input_actions.h"
#include "foundations/components.h"
#include "physics/physics_manager.h"
#include "game/foundations/body_layer_collisions.h"
#include <vector>

#include "glm/gtc/matrix_transform.hpp"

#include <cmath>

namespace
{
    ECS* s_ecs = nullptr;
    PhysicsManager* s_physics = nullptr;
    InGameCamRaycast s_raycast_settings = {};

    FPCamState s_fp_cam = {};
    TPCamState s_tp_cam = {};

    CameraInfo s_fp_camera = {};
    CameraInfo s_tp_camera = {};

    InGameCamGameplayMode s_gameplay_mode = InGameCamGameplayMode::TPCam;

    glm::vec3 s_movement_forward = glm::vec3(0.0f, 0.0f, -1.0f);

    bool  s_initialized = false;
    float s_occlusion_distance = 4.0f;

    constexpr float MOUSE_SENSITIVITY  = 0.10f;
    constexpr float GAMEPAD_LOOK_SPEED = 120.0f;
    constexpr float OCCLUSION_MIN_DISTANCE = 0.80f;
    constexpr float OCCLUSION_HIT_PADDING = 0.12f;
    constexpr float CAM_PULLING_SPEED = 35.0f;
    constexpr float CAM_PUSHING_SPEED = 12.0f;

    float SmoothExp(float current, float target, float dt, float speed)
    {
        if (dt <= 0.0f || speed <= 0.0f) return target;
        float alpha = 1.0f - expf(-speed * dt);
        return glm::mix(current, target, glm::clamp(alpha, 0.0f, 1.0f));
    }

    void FPCam_SyncFovFromRenderer(FPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

    void FPCam_ApplyFovToRenderer(const FPCamState& cam)
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

    void TPCam_SyncFovFromRenderer(TPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

    void TPCam_ApplyFovToRenderer(const TPCamState& cam)
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

    EntityID FindFirstBindablePlayer(ECS* ecs)
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

    CameraInfo UpdateFPCamera(FPCamState& cam, float dt, bool allow_look_input, bool apply_fov_to_renderer)
    {
        FPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            FPCam_ApplyFovToRenderer(cam);

        if (allow_look_input)
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY;

            cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
                * GAMEPAD_LOOK_SPEED * dt;
            cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
                * GAMEPAD_LOOK_SPEED * dt;
        }

        if (cam.pitch > 89.0f) cam.pitch = 89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

        C_Transform* bound_transform = nullptr;
        if (s_ecs && cam.bound_entity != NULL_ENTITY)
            bound_transform = s_ecs->GetComponentPtr<C_Transform>(cam.bound_entity);

        if (!bound_transform)
        {
            EntityID first_player = FindFirstBindablePlayer(s_ecs);
            if (first_player != NULL_ENTITY)
            {
                cam.bound_entity = first_player;
                bound_transform = s_ecs->GetComponentPtr<C_Transform>(cam.bound_entity);
            }
        }

        if (bound_transform)
        {
            glm::vec3 player_pos = glm::vec3(bound_transform->matrix[3]);
            cam.pos = player_pos + glm::vec3(0.0f, cam.eye_height, 0.0f);
        }

        return CameraInfo{
            .view            = glm::lookAt(cam.pos, cam.pos + cam.forward, glm::vec3(0.0f, 1.0f, 0.0f)),
            .position        = cam.pos,
            .lens_distortion = 0.0f
        };
    }

    CameraInfo UpdateTPCamera(TPCamState& cam, float dt, bool allow_look_input, bool apply_fov_to_renderer)
    {
        TPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            TPCam_ApplyFovToRenderer(cam);

        if (allow_look_input)
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY;

            cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
                * GAMEPAD_LOOK_SPEED * dt;
            cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
                * GAMEPAD_LOOK_SPEED * dt;
        }

        if (cam.pitch > 80.0f) cam.pitch = 80.0f;
        if (cam.pitch < -80.0f) cam.pitch = -80.0f;
        if (cam.distance < 0.5f) cam.distance = 0.5f;

        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

        C_Transform* bound_transform = nullptr;
        if (s_ecs && cam.bound_entity != NULL_ENTITY)
            bound_transform = s_ecs->GetComponentPtr<C_Transform>(cam.bound_entity);

        if (!bound_transform)
        {
            EntityID first_player = FindFirstBindablePlayer(s_ecs);
            if (first_player != NULL_ENTITY)
            {
                cam.bound_entity = first_player;
                bound_transform = s_ecs->GetComponentPtr<C_Transform>(cam.bound_entity);
            }
        }

        if (bound_transform)
        {
            glm::vec3 player_pos = glm::vec3(bound_transform->matrix[3]);
            cam.target = player_pos + glm::vec3(0.0f, cam.target_height, 0.0f);
        }

        float desired_distance = glm::max(cam.distance, OCCLUSION_MIN_DISTANCE);
        float target_distance = desired_distance;

        if (s_physics)
        {
            Ray ray = {};
            ray.origin = cam.target;
            ray.direction = glm::normalize(-cam.forward);
            ray.maxDistance = desired_distance;

            QueryFilterExternal filter = {};
            filter.bodyToIgnore = cam.bound_entity; // ignore the player
            filter.hasLayerOfQuery = s_raycast_settings.layered_query;
            filter.layerOfQuery = s_raycast_settings.layer;

            std::vector<EntityRaycastHit> hits = s_physics->raycastAll(ray, filter);

            float nearest_t = desired_distance;
            for (const EntityRaycastHit& hit : hits)
            {
                if (!hit.isValid()) continue;
                if (hit.entity == cam.bound_entity) continue;
                if (hit.t >= 0.0f && hit.t < nearest_t)
                    nearest_t = hit.t;
            }

            if (nearest_t < desired_distance)
            {
                target_distance = glm::clamp(
                    nearest_t - OCCLUSION_HIT_PADDING,
                    OCCLUSION_MIN_DISTANCE,
                    desired_distance
                );
            }
        }

        // remained for only static
        (void)s_raycast_settings.only_static;

        float speed = (target_distance < s_occlusion_distance)
            ? CAM_PULLING_SPEED
            : CAM_PUSHING_SPEED;

        s_occlusion_distance = SmoothExp(s_occlusion_distance, target_distance, dt, speed);
        cam.pos = cam.target - cam.forward * s_occlusion_distance;

                return CameraInfo{
                    .view            = glm::lookAt(cam.pos, cam.target, glm::vec3(0.0f, 1.0f, 0.0f)),
                    .position        = cam.pos,
                    .lens_distortion = 0.0f
        };
    }

    void RefreshMovementForward()
    {
        const bool gameplay_tp_active = s_gameplay_mode == InGameCamGameplayMode::TPCam;
        s_movement_forward = gameplay_tp_active ? s_tp_cam.forward : s_fp_cam.forward;
    }
}

void InGameCam_Init(ECS* ecs, PhysicsManager* physics, EntityID player_id, InGameCamRaycast raycast_settings)
{
    s_ecs = ecs;
    s_physics = physics;
    s_raycast_settings = raycast_settings;
    s_raycast_settings.layered_query = true;
    s_raycast_settings.layer = (uint8_t)BodyLayer::MOVING;
    s_raycast_settings.only_static = false; // reserved

    s_fp_cam = FPCamState{};
    s_tp_cam = TPCamState{};
    s_fp_camera = CameraInfo{};
    s_tp_camera = CameraInfo{};
    s_movement_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    s_gameplay_mode = InGameCamGameplayMode::TPCam;
    s_occlusion_distance = s_tp_cam.distance;

    if (player_id != NULL_ENTITY)
    {
        s_fp_cam.bound_entity = player_id;
        s_tp_cam.bound_entity = player_id;
    }

    const bool gameplay_fp_active = s_gameplay_mode == InGameCamGameplayMode::FPCam;
    const bool gameplay_tp_active = !gameplay_fp_active;

    s_fp_camera = UpdateFPCamera(s_fp_cam, 0.0f, false, gameplay_fp_active);
    s_tp_camera = UpdateTPCamera(s_tp_cam, 0.0f, false, gameplay_tp_active);

    RefreshMovementForward();

    s_initialized = true;
}

void InGameCam_Update(float dt, bool is_playing, bool debug_ui_open, bool right_mouse_down, DebugUICameraMode debug_camera_mode)
{
    if (!s_initialized) return;

    bool active_fp = false;
    bool active_tp = false;
    bool allow_look_input = false;

    if (!debug_ui_open)
    {
        active_fp = s_gameplay_mode == InGameCamGameplayMode::FPCam;
        active_tp = !active_fp;
        allow_look_input = is_playing;
    }
    else
    {
        switch (debug_camera_mode)
        {
            case DebugUICameraMode::FPCam:
                active_fp = true;
                allow_look_input = right_mouse_down;
                break;

            case DebugUICameraMode::TPCam:
                active_tp = true;
                allow_look_input = right_mouse_down;
                break;

            case DebugUICameraMode::FreeCam:
            default:
                break;
        }
    }

    s_fp_camera = UpdateFPCamera(s_fp_cam, dt, allow_look_input && active_fp, active_fp);
    s_tp_camera = UpdateTPCamera(s_tp_cam, dt, allow_look_input && active_tp, active_tp);

    RefreshMovementForward();
}

void InGameCam_SetGameplayMode(InGameCamGameplayMode mode)
{
    s_gameplay_mode = mode;
    const bool gameplay_fp_active = s_gameplay_mode == InGameCamGameplayMode::FPCam;
    const bool gameplay_tp_active = !gameplay_fp_active;

    // Keep renderer FOV aligned immediately after mode changes.
    s_fp_camera = UpdateFPCamera(s_fp_cam, 0.0f, false, gameplay_fp_active);
    s_tp_camera = UpdateTPCamera(s_tp_cam, 0.0f, false, gameplay_tp_active);

    RefreshMovementForward();
}

InGameCamGameplayMode InGameCam_GetGameplayMode()
{
    return s_gameplay_mode;
}

void InGameCam_ToggleGameplayMode()
{
    const InGameCamGameplayMode next_mode =
        (s_gameplay_mode == InGameCamGameplayMode::FPCam)
            ? InGameCamGameplayMode::TPCam
            : InGameCamGameplayMode::FPCam;

    InGameCam_SetGameplayMode(next_mode);
}

void InGameCam_ApplyDebugEdits(const InGameCamDebugEdits& edits)
{
    if (!s_initialized) return;

    if (edits.reset_fp)
    {
        const EntityID keep_bound = s_fp_cam.bound_entity;
        FPCam_ResetToDefault(s_fp_cam);
        s_fp_cam.bound_entity = keep_bound;
    }

    if (edits.reset_tp)
    {
        const EntityID keep_bound = s_tp_cam.bound_entity;
        TPCam_ResetToDefault(s_tp_cam);
        s_tp_cam.bound_entity = keep_bound;
    }

    if (edits.apply_fp_state)
        s_fp_cam = edits.fp_state;

    if (edits.apply_tp_state)
        s_tp_cam = edits.tp_state;
    
    if (edits.reset_tp || edits.apply_tp_state)
        s_occlusion_distance = glm::max(s_tp_cam.distance, OCCLUSION_MIN_DISTANCE);

    const bool gameplay_fp_active = s_gameplay_mode == InGameCamGameplayMode::FPCam;
    const bool gameplay_tp_active = !gameplay_fp_active;

    s_fp_camera = UpdateFPCamera(s_fp_cam, 0.0f, false, gameplay_fp_active);
    s_tp_camera = UpdateTPCamera(s_tp_cam, 0.0f, false, gameplay_tp_active);

    RefreshMovementForward();
}

InGameCamSnapshot InGameCam_GetSnapshot()
{
    InGameCamSnapshot snapshot = {};
    snapshot.valid = true;
    snapshot.gameplay_camera_mode =
        (s_gameplay_mode == InGameCamGameplayMode::FPCam)
            ? DebugUICameraMode::FPCam
            : DebugUICameraMode::TPCam;
    snapshot.fp_state = s_fp_cam;
    snapshot.tp_state = s_tp_cam;
    snapshot.fp_camera = s_fp_camera;
    snapshot.tp_camera = s_tp_camera;
    snapshot.has_fp_camera = true;
    snapshot.has_tp_camera = true;
    return snapshot;
}

const CameraInfo& InGameCam_GetFPCamera()
{
    return s_fp_camera;
}

const CameraInfo& InGameCam_GetTPCamera()
{
    return s_tp_camera;
}

const CameraInfo& InGameCam_GetGameplayCamera()
{
    return (s_gameplay_mode == InGameCamGameplayMode::FPCam) ? s_fp_camera : s_tp_camera;
}

glm::vec3 InGameCam_GetMovementForward()
{
    return s_movement_forward;
}

