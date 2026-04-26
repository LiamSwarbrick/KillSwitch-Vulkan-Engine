#ifndef DEBUG_UI_API_H
#define DEBUG_UI_API_H

#include "core/ecs.h"
#include "core/assetsys.h"
#include "renderer/renderer.h"   // CameraInfo
#include "glm/glm.hpp"

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
	float     target_height   = 1.50f;
	float     distance        = 4.00f;
	float     yaw             = -90.0f;
	float     pitch           = -15.0f;
	float     fov_deg         = 2864.8f;
	bool      fov_initialized = false;

	glm::vec3 pos             = { 0.0f, 2.0f, 4.0f };
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
void DebugUI_SetAsset(Asset* asset);

// Optional callback: called between ImGui::NewFrame() and ImGui::Render()
// Game code can set this to build its own ImGui UI
typedef void (*DebugUI_ImGuiBuildCallback)(void* user_data);
void DebugUI_SetImGuiCallback(DebugUI_ImGuiBuildCallback callback, void* user_data);

bool DebugUI_IsOpen();

// Debug camera mode is used by the Debug UI Camera panel (includes FreeCam).
void DebugUI_SetCameraMode(DebugUICameraMode mode);
DebugUICameraMode DebugUI_GetCameraMode();

// Gameplay camera mode is used when debug UI is hidden (FP/TP only).
void DebugUI_SetGameplayCameraMode(DebugUICameraMode mode);
DebugUICameraMode DebugUI_GetGameplayCameraMode();

// Game pushes/pulls FP camera state so debug UI can display and edit it.
void DebugUI_SetFPCamState(const FPCamState* state);
const FPCamState* DebugUI_GetFPCamState();

// Game pushes/pulls TP camera state so debug UI can display and edit it.
void DebugUI_SetTPCamState(const TPCamState* state);
const TPCamState* DebugUI_GetTPCamState();

// Game pushes the resolved FP camera each frame; debug UI may choose to use it.
void DebugUI_SetFPCamCameraInfo(const CameraInfo* camera);

// Game pushes the resolved TP camera each frame; debug UI may choose to use it.
void DebugUI_SetTPCamCameraInfo(const CameraInfo* camera);

// Resolve and return the active camera for this frame.
// Game should push FP/TP camera state/info each frame before calling this.
CameraInfo DebugUI_GetCameraInfo(float dt);

#endif  // DEBUG_UI_API_H
