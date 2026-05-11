#include "ingame_cam.h"

#include "core/components.h"
#include "core/input.h"
#include "core/input_actions.h"
#include "foundations/components.h"
#include "physics/physics_manager.h"
#include "physics/body_layers.h"
#include <vector>

#include "glm/gtc/matrix_transform.hpp"

#include <cmath>

namespace
{
    struct InGameCamOcclusionDetectSettings
    {
        bool layered_query = true;
        uint8_t layer = (int8_t)BodyLayer::AFFECT_ONLY_STATIC;
        bool only_static = false; // reserved

        bool shapecast = false;
        float shapecast_radius = 0.1f;
        float shapecast_padding = 0.05f;
    };
    InGameCamOcclusionDetectSettings s_occlusion_settings = {};
    ECS* s_ecs = nullptr;

    PhysicsManager* s_physics = nullptr;

    FPCamState s_fp_cam = {};
    TPCamState s_tp_cam = {};

    CameraInfo s_fp_camera = {};
    CameraInfo s_tp_camera = {};

    InGameCamGameplayMode s_gameplay_mode = InGameCamGameplayMode::TPCam;

    glm::vec3 s_movement_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 s_tp_follow_target = glm::vec3(0.0f);
    EntityID s_tp_follow_target_entity = NULL_ENTITY;
    float s_tp_shoulder_side_blend = 1.0f;
    const float near_plane = 0.1f;
    const float far_plane  = 200.0f;
    const float default_lens_distortion = -0.025f;

    bool  s_initialized = false;
    bool  s_tp_follow_target_initialized = false;
    float s_occlusion_distance = s_tp_cam.distance;
    float s_base_fov = TPCamState{}.fov_deg;
    float s_look_sensitivity_scale = 1.0f;
    ShapeHandle s_camera_probe_shape = InvalidShapeHandle;

    constexpr float MOUSE_SENSITIVITY  = 0.10f;
    constexpr float GAMEPAD_LOOK_SPEED = 120.0f;
    constexpr float OCCLUSION_MIN_DISTANCE = 0.80f;
    constexpr float OCCLUSION_HIT_PADDING = 0.12f;
    constexpr float CAM_PULLING_SPEED = 35.0f;
    constexpr float CAM_PUSHING_SPEED = 12.0f;
    constexpr float TP_SHOULDER_OFFSET = 0.5f;// 0.4f;
    constexpr float TP_SHOULDER_OFFSET_AIMING = 0.48f;
    constexpr float TP_FOLLOW_SNAP_DISTANCE = 4.0f;
    constexpr float TP_SHOULDER_BLEND_SPEED = 12.0f;
    constexpr float AIM_FOV_OFFSET = 700.0f;

    // Smooth function for cam
    float SmoothExp(float current, float target, float dt, float speed)
    {
        if (dt <= 0.0f || speed <= 0.0f) return target;
        float alpha = 1.0f - expf(-speed * dt);
        return glm::mix(current, target, glm::clamp(alpha, 0.0f, 1.0f));
    }
    // Smooth function for cam (vec3 version)
    glm::vec3 SmoothExpVec3(const glm::vec3& current, const glm::vec3& target, float dt, float speed)
    {
        if (dt <= 0.0f || speed <= 0.0f) return target;
        float alpha = 1.0f - expf(-speed * dt);
        return glm::mix(current, target, glm::clamp(alpha, 0.0f, 1.0f));
    }

    void ResetTPCamFollowTarget()
    {
        s_tp_follow_target = glm::vec3(0.0f);
        s_tp_follow_target_entity = NULL_ENTITY;
        s_tp_follow_target_initialized = false;
    }
    // helper to reset shoulder blend when toggling shoulder side
    void ResetTPCamShoulderBlend()
    {
        s_tp_shoulder_side_blend = s_tp_cam.shoulder_side_change ? -1.0f : 1.0f;
    }

    // Helper to destroy and recreate the camera probe shape when settings change
    void DestroyCameraProbeShape()
    {
        if (s_physics && s_camera_probe_shape.isValid())
        {
            s_physics->destroyShape(s_camera_probe_shape);
        }
        s_camera_probe_shape = InvalidShapeHandle;
    }

    void CreateCameraProbeShape()
    {
        DestroyCameraProbeShape();

        if (!s_physics || !s_occlusion_settings.shapecast) return;

        const float probe_radius = glm::max(0.01f, s_occlusion_settings.shapecast_radius);

        ShapeDesc probe_desc = ShapeDesc::makeSphere(probe_radius);
        probe_desc.localOffset = glm::vec3(0.0f);
        probe_desc.localOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        s_camera_probe_shape = s_physics->createShape(probe_desc);
    }


    // get fov from renderer and set to cam state
    void FPCam_SyncFovFromRenderer(FPCamState& cam)
    {
        if (cam.fov_initialized) return;

        Renderer_Settings settings = Renderer_GetSettings();
        cam.fov_deg = glm::degrees(settings.fov_y);
        cam.fov_initialized = true;
    }

    // apply cam state's fov to renderer
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

    // find first player entity
    EntityID FindFirstBindablePlayer(ECS* ecs)
    {
        if (!ecs) return NULL_ENTITY;

        EntityID found = NULL_ENTITY;
        ecs->GetView<C_PlayerInfo>().ForEach([&](EntityID id, C_PlayerInfo&)
        {
            if (found == NULL_ENTITY)
                found = id;
        });

        return found;
    }

    // Cam update
    // fpcam pos at eye_height
    CameraInfo UpdateFPCamera(FPCamState& cam, float dt, bool allow_look_input, bool apply_fov_to_renderer)
    {
        FPCam_SyncFovFromRenderer(cam);
        if (apply_fov_to_renderer)
            FPCam_ApplyFovToRenderer(cam);

        if (allow_look_input) // input allow in playstate, avoid input conflict
        {
            float mouse_dx = 0.0f;
            float mouse_dy = 0.0f;
            Input_GetMouseDelta(&mouse_dx, &mouse_dy);

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY * s_look_sensitivity_scale;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY * s_look_sensitivity_scale;

            cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
                * GAMEPAD_LOOK_SPEED * s_look_sensitivity_scale * dt;
            cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
                * GAMEPAD_LOOK_SPEED * s_look_sensitivity_scale * dt;
        }

        if (cam.pitch > 89.0f) cam.pitch = 89.0f; // cam pitch limit
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        // calculate forward from yaw/pitch
        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);

        // find bound entity transform
        C_Transform* bound_transform = nullptr;
        if (s_ecs && cam.bound_entity != NULL_ENTITY && s_ecs->IsEntityValid(cam.bound_entity))
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
            cam.pos = player_pos + glm::vec3(0.0f, cam.eye_height, 0.0f); // set cam pos to player pos + eye height
        }

        return CameraInfo{
            .view            = glm::lookAtRH(cam.pos, cam.pos + cam.forward, glm::vec3(0.0f, 1.0f, 0.0f)),
            .position        = cam.pos,
            .near_plane      = near_plane,
            .far_plane       = far_plane,
            .lens_distortion = default_lens_distortion
        };
    }
    // tpcam pos at target + forward * distance with offset, with occlusion detection, smooth following, aim downsight and shoulder toggle
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

            cam.yaw += mouse_dx * MOUSE_SENSITIVITY * s_look_sensitivity_scale;
            cam.pitch -= mouse_dy * MOUSE_SENSITIVITY * s_look_sensitivity_scale;
            // gamepad input
            cam.yaw += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT))
                * GAMEPAD_LOOK_SPEED * s_look_sensitivity_scale * dt;
            cam.pitch += (Input_GetActionValue(ACTION_CAMERA_UP) - Input_GetActionValue(ACTION_CAMERA_DOWN))
                * GAMEPAD_LOOK_SPEED * s_look_sensitivity_scale * dt;
        }

        if (cam.pitch > 80.0f) cam.pitch = 80.0f;
        if (cam.pitch < -80.0f) cam.pitch = -80.0f;
        if (cam.distance < 0.5f) cam.distance = 0.5f; // cam distance limit

        // compute forward from yaw/pitch
        glm::vec3 forward;
        forward.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        forward.y = sinf(glm::radians(cam.pitch));
        forward.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        cam.forward = glm::normalize(forward);
        // find bound entity transform
        C_Transform* bound_transform = nullptr;
        if (s_ecs && cam.bound_entity != NULL_ENTITY && s_ecs->IsEntityValid(cam.bound_entity))
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

        bool is_aiming = false;
        if (s_ecs && cam.bound_entity != NULL_ENTITY && s_ecs->IsEntityValid(cam.bound_entity))
        {
            if (const C_CombatInput* combatInput = s_ecs->GetComponentPtr<C_CombatInput>(cam.bound_entity))
                is_aiming = combatInput->wantsAim;
        }

        if (bound_transform)
        {   // calculate desired target pos
            glm::vec3 player_pos = glm::vec3(bound_transform->matrix[3]);
            float height = cam.target_height;
            if (s_ecs->Has<C_RigidBody>(cam.bound_entity) && s_physics)
            {
                Shape* genShape = s_physics->getShape(cam.bound_entity);
                height = genShape->getHeight(); // for character_capsule.gltf = 2m

                height = height * 0.5f * 0.7f; // tweak this
                player_pos.y += genShape->localOffset.y;
            }
            // target pos computation
            const glm::vec3 desired_target = player_pos + glm::vec3(0.0f, height, 0.0f);
            if (is_aiming)
            {
                s_tp_follow_target = desired_target;
                s_tp_follow_target_entity = cam.bound_entity;
                s_tp_follow_target_initialized = true;
            }
            else
            {
                const glm::vec3 target_delta = desired_target - s_tp_follow_target;
                const bool snap_follow_target = // snap when not initialized
                    !s_tp_follow_target_initialized ||
                    s_tp_follow_target_entity != cam.bound_entity ||
                    glm::dot(target_delta, target_delta) >=
                        (TP_FOLLOW_SNAP_DISTANCE * TP_FOLLOW_SNAP_DISTANCE);

                if (snap_follow_target)
                {
                    s_tp_follow_target = desired_target;
                    s_tp_follow_target_entity = cam.bound_entity;
                    s_tp_follow_target_initialized = true;
                }
                else
                {
                    const float follow_speed =
                        (cam.follow_lag_sec <= 0.0f) ? 0.0f : (1.0f / cam.follow_lag_sec);
                    s_tp_follow_target = SmoothExpVec3(
                        s_tp_follow_target,
                        desired_target,
                        dt,
                        follow_speed
                    );
                }
            }

            cam.target = s_tp_follow_target;
        }

        const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(cam.forward, up));

        const float target_shoulder_sign = s_tp_cam.shoulder_side_change ? -1.0f : 1.0f; // target shoulder side: left or right
        s_tp_shoulder_side_blend = SmoothExp(
            s_tp_shoulder_side_blend,
            target_shoulder_sign,
            dt,
            TP_SHOULDER_BLEND_SPEED
        );

        const float shoulder_offset = TP_SHOULDER_OFFSET * s_tp_shoulder_side_blend;

        float desired_distance = glm::max(cam.distance, OCCLUSION_MIN_DISTANCE);
        float target_distance = desired_distance;
        // occlusion detection
        if (s_physics)
        {
            Ray ray = {};
            ray.origin = cam.target + right * shoulder_offset; // ray origin at target with shoulder offset
            ray.direction = glm::normalize(-cam.forward);
            ray.maxDistance = desired_distance;

            QueryFilterExternal filter = {};
            if (cam.bound_entity != NULL_ENTITY && s_ecs->IsEntityValid(cam.bound_entity))
            {
                filter.bodyToIgnore = cam.bound_entity;
            }// ignore the player
            filter.hasLayerOfQuery = s_occlusion_settings.layered_query;
            filter.layerOfQuery = s_occlusion_settings.layer;

            // raycast: line-of-sight constraint
            float distance_by_ray = desired_distance;
            {
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
                    distance_by_ray = glm::clamp(nearest_t - OCCLUSION_HIT_PADDING,
                                                 OCCLUSION_MIN_DISTANCE, desired_distance);
            }

            // shapecast: camera sphere volume constraint
            float distance_by_shape = desired_distance;
            if (s_occlusion_settings.shapecast && s_camera_probe_shape.isValid())
            {
                EntityShapecastHit sc_hit = s_physics->shapecast(
                    ray, s_camera_probe_shape, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), filter);

                if (sc_hit.isValid() && sc_hit.entity != cam.bound_entity)
                    distance_by_shape = glm::clamp(
                        sc_hit.t - s_occlusion_settings.shapecast_padding,
                        OCCLUSION_MIN_DISTANCE, desired_distance);
            }

            target_distance = glm::min(distance_by_ray, distance_by_shape);
        }

        
        // speed of camera adjustment
        float speed = (target_distance < s_occlusion_distance)
            ? CAM_PULLING_SPEED
            : CAM_PUSHING_SPEED;

        s_occlusion_distance = SmoothExp(s_occlusion_distance, target_distance, dt, speed);

        // offset computation
        cam.pos = cam.target - cam.forward * s_occlusion_distance + right * shoulder_offset;

        // Aiming offset and FOV change
        const float target_aim_offset = is_aiming ? TP_SHOULDER_OFFSET_AIMING : TP_SHOULDER_OFFSET;
        const float aimed_fov = glm::max(40.0f, s_base_fov - AIM_FOV_OFFSET);
        const float target_fov = is_aiming ? aimed_fov : s_base_fov;

        // Smooth shoulder aim offset
        static float SHOULDER_OFFSET = TP_SHOULDER_OFFSET;
        SHOULDER_OFFSET = SmoothExp(SHOULDER_OFFSET, target_aim_offset, dt, 2.0f);

        // Smooth FOV
        cam.fov_deg = SmoothExp(cam.fov_deg, target_fov, dt, 2.0f);
        cam.fov_initialized = true;

        glm::vec3 aim_target = cam.target + right * (SHOULDER_OFFSET * s_tp_shoulder_side_blend);
        return CameraInfo{
                    .view            = glm::lookAtRH(cam.pos, aim_target, glm::vec3(0.0f, 1.0f, 0.0f)),
                    .position        = cam.pos,
                    .near_plane      = near_plane,
                    .far_plane       = far_plane,
                    .lens_distortion = default_lens_distortion,
                    .screenshake     = 0.0f  // TODO: Change this.
        };
    }

    void RefreshMovementForward()
    {
        const bool gameplay_tp_active = s_gameplay_mode == InGameCamGameplayMode::TPCam;
        s_movement_forward = gameplay_tp_active ? s_tp_cam.forward : s_fp_cam.forward;
    }
}

void InGameCam_Init(ECS* ecs, PhysicsManager* physics, EntityID player_id)
{

    s_ecs = ecs;
    s_physics = physics;

    s_occlusion_settings.layered_query = true;
    s_occlusion_settings.layer = (uint8_t)BodyLayer::AFFECT_NOT_CHARACTER;
    s_occlusion_settings.shapecast = true;

    CreateCameraProbeShape();

    s_fp_cam = FPCamState{};
    s_tp_cam = TPCamState{};
    s_fp_camera = CameraInfo{};
    s_tp_camera = CameraInfo{};
    s_movement_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    s_gameplay_mode = InGameCamGameplayMode::TPCam;
    ResetTPCamFollowTarget();
    ResetTPCamShoulderBlend();
    s_occlusion_distance = s_tp_cam.distance;
    s_base_fov = s_tp_cam.fov_deg;

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

void InGameCam_ToggleShoulder()
{
    s_tp_cam.shoulder_side_change = !s_tp_cam.shoulder_side_change;
}

void InGameCam_SetLookSensitivity(float sensitivity_scale)
{
    s_look_sensitivity_scale = glm::max(sensitivity_scale, 0.01f);
}

float InGameCam_GetLookSensitivity()
{
    return s_look_sensitivity_scale;
}

void InGameCam_Shutdown()
{
    DestroyCameraProbeShape();
    ResetTPCamFollowTarget();

    s_ecs = nullptr;
    s_physics = nullptr;
    s_initialized = false;
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
        ResetTPCamFollowTarget();
        ResetTPCamShoulderBlend();
        s_base_fov = edits.tp_state.fov_deg;
    }

    if (edits.apply_fp_state)
        s_fp_cam = edits.fp_state;

    if (edits.apply_tp_state)
    {
        s_tp_cam = edits.tp_state;
        s_base_fov = edits.tp_state.fov_deg;
    }
    
    if (edits.reset_tp || edits.apply_tp_state)
    {
        ResetTPCamFollowTarget();
        s_occlusion_distance = glm::max(s_tp_cam.distance, OCCLUSION_MIN_DISTANCE);
    }

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

