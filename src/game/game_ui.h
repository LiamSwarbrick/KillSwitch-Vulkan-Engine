#pragma once

//  Usage (in main loop):
//    GameUI_Init();
//    // per-frame:
//    GameUI_Update();           // handle input shortcuts (ESC to pause etc.)
//    GameUI_BuildImGui();       // call inside DebugUI_SetImGuiCallback
//    GameState s = GameUI_GetState();

struct ImFont; // Forward declaration — callers include imgui.h to use ImFont*

// Named font slots. Load a TTF into a slot via GameUI_LoadFont().
// Any unloaded slot returns nullptr from GameUI_GetFont(),
// which is valid as ImGui::PushFont(nullptr) uses the built-in default.
enum class GameFont
{
    Default = 0, // Always nullptr — resolves to ImGui built-in default
    Title,       // Large / decorative heading font
    Body,        // Normal menu / HUD text
    Mono,        // Monospace (debug labels, captions)
    Count,
};

enum class GameState
{
    MainMenu,
    Playing,
    Paused,
    Quitting,
};

// Register a TTF font into a named slot. Must be called after Renderer_Init()
// and before the first rendered frame. Returns false if the file cannot be loaded.
bool      GameUI_LoadFont(GameFont slot, const char* ttf_path, float size_pixels);

// Returns the ImFont* for the given slot, or nullptr (= ImGui built-in default).
ImFont*   GameUI_GetFont(GameFont slot);

void      GameUI_Init();
void      GameUI_Update();          // call once per frame (handles ACTION_PAUSE etc.)
void      GameUI_BuildImGui();      // call from ImGui callback
GameState GameUI_GetState();
void      GameUI_SetState(GameState state);
