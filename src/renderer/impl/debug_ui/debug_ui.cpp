#include "debug_ui.h"
#include "renderer/debug_ui_api.h"

DebugUI::DebugUIState debug_ui_state;
AdvEng::ECS*          debug_ecs_ptr   = nullptr;
Asset*                debug_asset_ptr = nullptr;

void DebugUI::SetECS(AdvEng::ECS* ecs)
{
    debug_ecs_ptr = ecs;
}

void DebugUI::SetAsset(Asset* asset)
{
    debug_asset_ptr = asset;
    debug_ui_state.debug_asset = asset;
}

