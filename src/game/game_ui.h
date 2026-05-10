#pragma once

//  Usage (in main loop):
//    GameUI_Init();
//    // per-frame:
//    GameUI_Update();           // handle input shortcuts (ESC to pause etc.)
//    GameUI_BuildImGui();       // call inside DebugUI_SetImGuiCallback
//    GameState s = GameUI_GetState();

class Scene;
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
    GameOver,
    Quitting,
};

constexpr int LEVEL_START_SKILL_OPTION_COUNT = 3;

struct LevelStartSkillOption
{
    const char* skill_id = nullptr;
    const char* display_name = "Placeholder Skill";
    const char* description = "TODO: hook this skill into Scene/ECS/animation logic.";
};

struct LevelStartSkillSelection
{
    int level_index = 0;
    int selected_index = -1;
    LevelStartSkillOption selected_option = {};
};

struct GameUIPlayingHUDState
{
    int life_count = 0;
    int loaded_bullets = 0;
    int backup_bullets = 0;
};

using LevelStartSkillApplyCallback = void (*)(Scene& scene, const LevelStartSkillSelection& selection);

// Register a TTF font into a named slot. Must be called after Renderer_Init()
// and before the first rendered frame. Returns false if the file cannot be loaded.
bool      GameUI_LoadFont(GameFont slot, const char* ttf_path, float size_pixels);

// Returns the ImFont* for the given slot, or nullptr (= ImGui built-in default).
ImFont*   GameUI_GetFont(GameFont slot);

// Opens the level-start skill choice modal with 3 options.
// If options == nullptr, a built-in placeholder skill list is used.
void      GameUI_OpenLevelStartSkillSelection(int level_index, const LevelStartSkillOption* options = nullptr);

// True while the skill choice modal is visible and gameplay should be blocked.
bool      GameUI_IsLevelStartSkillSelectionOpen();

// Register a Scene-typed callback where gameplay modifications should be applied.
void      GameUI_SetLevelStartSkillApplyCallback(Scene* scene, LevelStartSkillApplyCallback callback);

// Update the persistent playing HUD values that get rendered over gameplay.
void      GameUI_SetPlayingHUDState(const GameUIPlayingHUDState& state);

// Trigger the short damage vignette used when the player gets hit.
void      GameUI_TriggerDamageFlash();

void      GameUI_Init();
void      GameUI_Update();          // call once per frame (handles ACTION_PAUSE etc.)
void      GameUI_BuildImGui();      // call from ImGui callback
GameState GameUI_GetState();
void      GameUI_SetState(GameState state);
