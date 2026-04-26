#include "debug_ui.h"
#include "renderer/debug_ui_api.h"

DebugUI::DebugUIState debug_ui_state;
ECS*                  debug_ecs_ptr   = nullptr;
Asset*                debug_asset_ptr = nullptr;

void DebugUI_SetECS(ECS* ecs)
{
    debug_ecs_ptr = ecs;
}

void DebugUI_SetAsset(Asset* asset)
{
    debug_asset_ptr = asset;
    debug_ui_state.debug_asset = asset;
}

void DebugUI_SetImGuiCallback(DebugUI_ImGuiBuildCallback callback, void* user_data)
{
    renderstate.imgui_callback      = callback;
    renderstate.imgui_callback_data = user_data;
}

bool DebugUI_IsOpen()
{
    return debug_ui_state.show_debug_ui;
}

void DebugUI_SetFPCamState(const FPCamState* state)
{
    if (!state) return;
    debug_ui_state.fp_cam = *state;
}

const FPCamState* DebugUI_GetFPCamState()
{
    return &debug_ui_state.fp_cam;
}

void DebugUI_SetFPCamCameraInfo(const CameraInfo* camera)
{
    if (!camera)
    {
        debug_ui_state.has_fp_camera = false;
        return;
    }

    debug_ui_state.fp_camera = *camera;
    debug_ui_state.has_fp_camera = true;
}

CameraInfo DebugUI_GetCameraInfo(float dt)
{
    // Gameplay always uses FP camera when debug UI is hidden.
    if (!debug_ui_state.show_debug_ui)
    {
        if (debug_ui_state.has_fp_camera)
            return debug_ui_state.fp_camera;

        return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
    }

    // Debug mode defaults to free cam, but can be switched in Camera panel.
    switch (debug_ui_state.camera_mode)
    {
        case DebugUI::CameraMode::FPCam:
            if (debug_ui_state.has_fp_camera)
                return debug_ui_state.fp_camera;
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUI::CameraMode::TPCam:
            // Temporary fallback until TP cam implementation lands.
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUI::CameraMode::FreeCam:
        default:
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
    }
}
