#pragma once

//  Usage (in main loop):
//    GameUI_Init();
//    // per-frame:
//    GameUI_Update();           // handle input shortcuts (ESC to pause etc.)
//    GameUI_BuildImGui();       // call inside DebugUI_SetImGuiCallback
//    GameState s = GameUI_GetState();

enum class GameState
{
    MainMenu,
    Playing,
    Paused,
    Quitting,
};

void      GameUI_Init();
void      GameUI_Update();          // call once per frame (handles ACTION_PAUSE etc.)
void      GameUI_BuildImGui();      // call from ImGui callback
GameState GameUI_GetState();
void      GameUI_SetState(GameState state);
