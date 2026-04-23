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

CameraInfo DebugUI_GetCameraInfo(float dt)
{
    return DebugUI::FreeCam_Update(debug_ui_state.free_cam, dt);
}
