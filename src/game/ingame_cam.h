#pragma once

#include "renderer/debug_ui_api.h"
#include "renderer/renderer.h"
#include "core/ecs.h"
#include "glm/glm.hpp"
#include "physics/physics_manager.h"

enum class InGameCamGameplayMode
{
    FPCam,
    TPCam
};

using InGameCamDebugEdits = DebugUICameraEdits;
using InGameCamSnapshot = DebugUIInGameCameraSnapshot;
struct InGameCamRaycast
{
    bool layered_query = true;
    uint8_t layer = 0;
    bool only_static = false;
};

void InGameCam_Init(ECS* ecs, PhysicsManager* physics, EntityID player_id, InGameCamRaycast raycast_settings = {});
void InGameCam_Update(
    float dt, bool is_playing, bool debug_ui_open, bool right_mouse_down,
    DebugUICameraMode debug_camera_mode
);
void InGameCam_SetGameplayMode(InGameCamGameplayMode mode);
InGameCamGameplayMode InGameCam_GetGameplayMode();
void InGameCam_ToggleGameplayMode();

void InGameCam_ApplyDebugEdits(const InGameCamDebugEdits& edits);
InGameCamSnapshot InGameCam_GetSnapshot();

const CameraInfo& InGameCam_GetFPCamera();
const CameraInfo& InGameCam_GetTPCamera();
const CameraInfo& InGameCam_GetGameplayCamera();
glm::vec3 InGameCam_GetMovementForward();