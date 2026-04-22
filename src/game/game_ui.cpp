#include "game_ui.h"
#include "core/input.h"

#include "imgui.h"

// ============================================================
//  Internal state
// ============================================================

static GameState s_state = GameState::MainMenu;

// ============================================================
//  Helpers — shared style
// ============================================================

// Push a consistent button style: transparent background, white text,
// with a subtle hover highlight. Easy to replace with custom art later.
static void PushMenuButtonStyle()
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(20.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
}

static void PopMenuButtonStyle()
{
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

// Invisible, no-border window helper
static bool BeginOverlayWindow(const char* id, ImVec2 pos, ImVec2 size,
                               ImGuiWindowFlags extra_flags = 0)
{
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoInputs  // removed per-window; handled manually
                           | extra_flags;
    // Remove NoInputs so buttons work
    flags &= ~ImGuiWindowFlags_NoInputs;
    return ImGui::Begin(id, nullptr, flags);
}

// ============================================================
//  Main Menu
// ============================================================

static void DrawMainMenu()
{
    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    // Full-screen black background
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 255));

    // --- Title (upper center) ---
    const char* title = "ADVENTURE ENGINE";
    ImGui::PushFont(nullptr);  // replace nullptr with large font when available
    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.22f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##title", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(2.5f);
    ImGui::TextUnformatted(title);
    ImGui::End();
    ImGui::PopFont();

    // --- Buttons: centered horizontally, lower third of screen ---
    const float btn_w   = 220.0f;
    const float btn_h   =  48.0f;
    const float gap     =  16.0f;
    const float pad_v   =  12.0f;  // top/bottom padding inside window
    const float total_h = pad_v * 2 + btn_h * 3 + gap * 2;

    float bx = sw * 0.5f - btn_w * 0.5f;
    float by = sh * 0.65f;

    ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btn_w, total_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, pad_v));
    ImGui::Begin("##mainmenu_btns", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    PushMenuButtonStyle();

    if (ImGui::Button("Start Game", ImVec2(btn_w, btn_h)))
        s_state = GameState::Playing;

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings",   ImVec2(btn_w, btn_h)))
    { /* TODO: open settings panel */ }

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit",       ImVec2(btn_w, btn_h)))
        s_state = GameState::Quitting;

    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleVar();  // WindowPadding
}

// ============================================================
//  Pause Menu
// ============================================================

static void DrawPauseMenu()
{
    ImGuiIO& io = ImGui::GetIO();

    // Semi-transparent dark overlay over 3D scene
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 140));

    // --- Panel anchored to top-left ---
    const float pad    = 24.0f;
    const float btn_w  = 200.0f;
    const float btn_h  =  42.0f;
    const float gap    =  10.0f;
    const float panel_h = btn_h * 4 + gap * 3 + pad * 2 + 28.0f; // 28 for title
    const float panel_w = btn_w + pad * 2;

    ImGui::SetNextWindowPos(ImVec2(30.0f, 30.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,   ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

    ImGui::Begin("##pausemenu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    // Title
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("PAUSED");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    PushMenuButtonStyle();

    if (ImGui::Button("Continue",           ImVec2(btn_w, btn_h)))
        s_state = GameState::Playing;

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings",           ImVec2(btn_w, btn_h)))
    { /* TODO: open settings panel */ }

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Return to Main Menu", ImVec2(btn_w, btn_h)))
        s_state = GameState::MainMenu;

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit Game",          ImVec2(btn_w, btn_h)))
        s_state = GameState::Quitting;

    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ============================================================
//  Public API
// ============================================================

void GameUI_Init()
{
    s_state = GameState::MainMenu;
}

void GameUI_Update()
{
    // ESC — toggle pause while playing, or resume from pause
    if (Input_IsActionJustPressed(ACTION_PAUSE))
    {
        if (s_state == GameState::Playing)
            s_state = GameState::Paused;
        else if (s_state == GameState::Paused)
            s_state = GameState::Playing;
    }
}

void GameUI_BuildImGui()
{
    switch (s_state)
    {
    case GameState::MainMenu: DrawMainMenu(); break;
    case GameState::Paused:   DrawPauseMenu(); break;
    default: break;  // Playing / Quitting: no UI drawn here
    }
}

GameState GameUI_GetState()
{
    return s_state;
}

void GameUI_SetState(GameState state)
{
    s_state = state;
}
