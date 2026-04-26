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

void DebugUI_SetCameraMode(DebugUICameraMode mode)
{
    debug_ui_state.camera_mode = mode;
}

DebugUICameraMode DebugUI_GetCameraMode()
{
    return debug_ui_state.camera_mode;
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

void DebugUI_SetTPCamState(const TPCamState* state)
{
    if (!state) return;
    debug_ui_state.tp_cam = *state;
}

const TPCamState* DebugUI_GetTPCamState()
{
    return &debug_ui_state.tp_cam;
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

void DebugUI_SetTPCamCameraInfo(const CameraInfo* camera)
{
    if (!camera)
    {
        debug_ui_state.has_tp_camera = false;
        return;
    }

    debug_ui_state.tp_camera = *camera;
    debug_ui_state.has_tp_camera = true;
}

CameraInfo DebugUI_GetCameraInfo(float dt)
{
    // Gameplay camera follows the currently selected non-debug camera mode.
    if (!debug_ui_state.show_debug_ui)
    {
        switch (debug_ui_state.camera_mode)
        {
            case DebugUICameraMode::FPCam:
                if (debug_ui_state.has_fp_camera)
                    return debug_ui_state.fp_camera;
                return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

            case DebugUICameraMode::TPCam:
                if (debug_ui_state.has_tp_camera)
                    return debug_ui_state.tp_camera;
                return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

            case DebugUICameraMode::FreeCam:
            default:
                return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
        }
    }

    // Debug mode defaults to free cam, but can be switched in Camera panel.
    switch (debug_ui_state.camera_mode)
    {
        case DebugUICameraMode::FPCam:
            if (debug_ui_state.has_fp_camera)
                return debug_ui_state.fp_camera;
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUICameraMode::TPCam:
            if (debug_ui_state.has_tp_camera)
                return debug_ui_state.tp_camera;
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUICameraMode::FreeCam:
        default:
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
    }
}
