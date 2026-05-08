#ifndef DEBUG_UI_API_H
#define DEBUG_UI_API_H

#include "core/ecs.h"
#include "core/assetsys.h"
#include "renderer/renderer.h"   // CameraInfo
#include "glm/glm.hpp"
#include <vector>

// Shared game/debug FP camera state.
struct FPCamState
{
	EntityID  bound_entity    = NULL_ENTITY;
	float     eye_height      = 1.70f;
	float     yaw             = -90.0f;
	float     pitch           = 0.0f;
	float     fov_deg         = 2864.8f;
	bool      fov_initialized = false;

	glm::vec3 pos             = { 0.0f, 1.7f, 0.0f };
	glm::vec3 forward         = { 0.0f, 0.0f, -1.0f };
};

// Shared game/debug TP camera state.
struct TPCamState
{
	EntityID  bound_entity    = NULL_ENTITY;
	float     target_height   = 1.4f;
	float     distance        = 2.00f;  // 1.0f
	float     follow_lag_sec  = 0.10f;
	float     yaw             = -90.0f;
	float     pitch           = -15.0f;
	float     fov_deg         = 2864.8f;
	bool      fov_initialized = false;
	bool	  shoulder_side_change = false;

	glm::vec3 pos             = { 0.0f, 2.0f, 1.0f };
	glm::vec3 target          = { 0.0f, 1.5f, 0.0f };
	glm::vec3 forward         = { 0.0f, 0.0f, -1.0f };
};

// Shared camera mode between game logic and debug UI camera selection.
enum class DebugUICameraMode
{
	FreeCam,
	FPCam,
	TPCam
};

inline void FPCam_ResetToDefault(FPCamState& cam)
{
	cam = FPCamState{};
	cam.fov_initialized = true;
}

inline void TPCam_ResetToDefault(TPCamState& cam)
{
	cam = TPCamState{};
	cam.fov_initialized = true;
}

void DebugUI_SetECS(ECS* ecs);
void DebugUI_SetAsset(std::vector<Asset*>* prefabs);

// Optional callback: called between ImGui::NewFrame() and ImGui::Render()
// Game code can set this to build its own ImGui UI
typedef void (*DebugUI_ImGuiBuildCallback)(void* user_data);
void DebugUI_SetImGuiCallback(DebugUI_ImGuiBuildCallback callback, void* user_data);

bool DebugUI_IsOpen();

// Debug camera mode is used by the Debug UI Camera panel (includes FreeCam).
void DebugUI_SetCameraMode(DebugUICameraMode mode);
DebugUICameraMode DebugUI_GetCameraMode();

// Camera snapshot pushed from game-owned camera system.
struct DebugUIInGameCameraSnapshot
{
	bool              valid = false;
	DebugUICameraMode gameplay_camera_mode = DebugUICameraMode::TPCam;

	FPCamState        fp_state = {};
	TPCamState        tp_state = {};

	CameraInfo        fp_camera = {};
	CameraInfo        tp_camera = {};
	bool              has_fp_camera = false;
	bool              has_tp_camera = false;
};

// Camera edit commands emitted by debug UI and consumed by game camera system.
struct DebugUICameraEdits
{
	bool      apply_fp_state = false;
	FPCamState fp_state = {};
	bool      apply_tp_state = false;
	TPCamState tp_state = {};
	bool      reset_fp = false;
	bool      reset_tp = false;
};

void DebugUI_SetInGameCameraSnapshot(const DebugUIInGameCameraSnapshot* snapshot);
bool DebugUI_ConsumeCameraEdits(DebugUICameraEdits* out_edits);

// Resolve and return the active camera for this frame.
// Uses game snapshot for FP/TP and debug-owned free cam for FreeCam mode.
CameraInfo DebugUI_GetCameraInfo(float dt);

#endif  // DEBUG_UI_API_H
