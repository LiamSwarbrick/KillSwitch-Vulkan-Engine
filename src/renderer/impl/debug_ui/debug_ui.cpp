#include "debug_ui.h"
#include "renderer/debug_ui_api.h"

DebugUI::DebugUIState debug_ui_state;
ECS*                  debug_ecs_ptr   = nullptr;
Asset*                debug_asset_ptr = nullptr;

void DebugUI_SetECS(ECS* ecs)
{
    debug_ecs_ptr = ecs;
}

void DebugUI_SetAsset(std::vector<Asset*>* prefabs)
{
    debug_ui_state.asset_list = prefabs;

    if (prefabs == nullptr || prefabs->empty())
    {
        debug_asset_ptr = nullptr;
        debug_ui_state.debug_asset = nullptr;
        return;
    }

    debug_asset_ptr = (*prefabs)[0];
    debug_ui_state.debug_asset = (*prefabs)[0];
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

void DebugUI_SetInGameCameraSnapshot(const DebugUIInGameCameraSnapshot* snapshot)
{
    if (!snapshot)
    {
        debug_ui_state.ingame_camera_snapshot = DebugUIInGameCameraSnapshot{};
        return;
    }

    debug_ui_state.ingame_camera_snapshot = *snapshot;
}

bool DebugUI_ConsumeCameraEdits(DebugUICameraEdits* out_edits)
{
    if (!debug_ui_state.has_pending_camera_edits)
        return false;

    if (out_edits)
        *out_edits = debug_ui_state.pending_camera_edits;

    debug_ui_state.pending_camera_edits = DebugUICameraEdits{};
    debug_ui_state.has_pending_camera_edits = false;
    return true;
}

CameraInfo DebugUI_GetCameraInfo(float dt)
{
    const DebugUIInGameCameraSnapshot& snapshot = debug_ui_state.ingame_camera_snapshot;

    // Gameplay camera follows dedicated gameplay mode (FP/TP) when debug UI is hidden.
    if (!debug_ui_state.show_debug_ui)
    {
        if (snapshot.valid)
        {
            switch (snapshot.gameplay_camera_mode)
            {
                case DebugUICameraMode::FPCam:
                    if (snapshot.has_fp_camera)
                        return snapshot.fp_camera;
                    break;

                case DebugUICameraMode::TPCam:
                default:
                    if (snapshot.has_tp_camera)
                        return snapshot.tp_camera;
                    break;
            }
        }

        return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
    }

    // Debug mode defaults to free cam, but can be switched in Camera panel.
    switch (debug_ui_state.camera_mode)
    {
        case DebugUICameraMode::FPCam:
            if (snapshot.valid && snapshot.has_fp_camera)
                return snapshot.fp_camera;
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUICameraMode::TPCam:
            if (snapshot.valid && snapshot.has_tp_camera)
                return snapshot.tp_camera;
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);

        case DebugUICameraMode::FreeCam:
        default:
            return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
    }
}
