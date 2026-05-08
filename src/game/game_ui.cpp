#include "game_ui.h"
#include "core/input.h"
#include "renderer/debug_ui_api.h"

#include "imgui.h"

// ============================================================
//  Internal state
// ============================================================

static GameState s_state = GameState::MainMenu;

// Main menu image assets (loaded once at startup)
static Renderer_UITexture s_mm_title_tex  = {};
static Renderer_UITexture s_mm_start_tex  = {};
static Renderer_UITexture s_mm_option_tex = {};
static Renderer_UITexture s_mm_exit_tex   = {};

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

// Returns true if the given UI texture is valid and can be drawn.
static bool HasUITexture(const Renderer_UITexture& tex)
{
    return tex.imgui_texture_id != 0 && tex.width > 0 && tex.height > 0;
}

// Compute the height for a button with the given width, based on the aspect ratio of the texture.
static float ComputeButtonHeight(const Renderer_UITexture& tex, float button_width, float fallback_height)
{
    if (!HasUITexture(tex))
        return fallback_height;

    return button_width * ((float)tex.height / (float)tex.width);
}

// Draw an image button if the texture is valid, otherwise fall back to a regular button with text.
static bool DrawMenuImageButtonOrFallback(
    const char* image_id,
    const Renderer_UITexture& tex,
    const char* fallback_label, // Text to show if texture is not available
    float button_width,
    float fallback_height)
{
    if (!HasUITexture(tex))
        return ImGui::Button(fallback_label, ImVec2(button_width, fallback_height));

    float button_height = ComputeButtonHeight(tex, button_width, fallback_height);

    // some artstyle for image buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 1, 1, 0.22f));

    bool pressed = ImGui::ImageButton(
        image_id,
        (ImTextureID)tex.imgui_texture_id,
        ImVec2(button_width, button_height)
    );

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return pressed;
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
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 122));

    // --- Title (upper center) ---
    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.22f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##title", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    // Draw the title image if available, otherwise fall back to text
    if (HasUITexture(s_mm_title_tex))
    {
        float max_w = sw * 0.62f;
        float draw_w = max_w;
        float draw_h = draw_w * ((float)s_mm_title_tex.height / (float)s_mm_title_tex.width);
        ImGui::Image((ImTextureID)s_mm_title_tex.imgui_texture_id, ImVec2(draw_w, draw_h));
    }
    else
    {
        ImGui::SetWindowFontScale(2.5f);
        ImGui::TextUnformatted("HELL MIST");
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // --- Buttons: centered horizontally, lower third of screen ---
    const float btn_w   = 260.0f; // button width
    const float btn_h   =  48.0f; // button height
    const float gap     =  16.0f; // gap between buttons
    const float pad_v   =  2.0f;  // top/bottom padding inside window
    const float h_start = ComputeButtonHeight(s_mm_start_tex, btn_w, btn_h); 
    const float h_opt   = ComputeButtonHeight(s_mm_option_tex, btn_w, btn_h);
    const float h_exit  = ComputeButtonHeight(s_mm_exit_tex, btn_w, btn_h);
    const float total_h = pad_v * 2 + h_start + h_opt + h_exit + gap * 2;

    float bx = sw * 0.5f - btn_w * 0.5f;// center horizontally
    float by = sh * 0.5f;// start vertically at lower middle of screen

    ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btn_w, total_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, pad_v));
    ImGui::Begin("##mainmenu_btns", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    PushMenuButtonStyle();

    // state transitions triggered by buttons:
    if (DrawMenuImageButtonOrFallback("##btn_start", s_mm_start_tex, "Start Game", btn_w, btn_h))
        s_state = GameState::Playing;

    ImGui::Dummy(ImVec2(0, gap));

    if (DrawMenuImageButtonOrFallback("##btn_options", s_mm_option_tex, "Settings", btn_w, btn_h))
    { /* TODO: open settings panel */ }

    ImGui::Dummy(ImVec2(0, gap));

    if (DrawMenuImageButtonOrFallback("##btn_exit", s_mm_exit_tex, "Quit", btn_w, btn_h))
        s_state = GameState::Quitting;

    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);  // WindowPadding
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

    // Main menu art (fallbacks are handled automatically in DrawMainMenu).
    Renderer_LoadUITexture("assets/UI/MainMenu/title.png", false, &s_mm_title_tex);
    Renderer_LoadUITexture("assets/UI/MainMenu/Start button.png", false, &s_mm_start_tex);
    Renderer_LoadUITexture("assets/UI/MainMenu/option.png", false, &s_mm_option_tex);
    Renderer_LoadUITexture("assets/UI/MainMenu/exit.png", false, &s_mm_exit_tex);
}

void GameUI_Update()
{
    // While Debug UI is open, freeze game UI state transitions.
    // This keeps game UI hidden and restores its previous state when debug UI closes.
    if (DebugUI_IsOpen())
        return;

    // ESC — toggle pause while playing, or resume from pause
    if (Input_IsActionJustPressed(ACTION_PAUSE))
    {
        if (s_state == GameState::Playing)
            s_state = GameState::Paused;
        else if (s_state == GameState::Paused)
            s_state = GameState::Playing;
    }
}


static void DrawCrosshair()
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float arm = 8.0f;
    const float gap = 4.0f;
    const float thickness = 2.0f;
    const ImU32 col = IM_COL32(255, 255, 255, 220);

    // dl->AddLine(ImVec2(c.x - gap - arm, c.y), ImVec2(c.x - gap, c.y), col, thickness);
    // dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + arm, c.y), col, thickness);
    // dl->AddLine(ImVec2(c.x, c.y - gap - arm), ImVec2(c.x, c.y - gap), col, thickness);
    // dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + arm), col, thickness);
    dl->AddCircleFilled(c, 3.0f, col);
}

void GameUI_BuildImGui()
{
    // When the debug UI is open the game viewport is a sub-window;
    // skip fullscreen overlays so the 3D scene stays visible there.
    if (DebugUI_IsOpen()) return;

    switch (s_state)
    {
    case GameState::MainMenu: DrawMainMenu(); break;
    case GameState::Paused:   DrawPauseMenu(); break;
    case GameState::Playing: 
        if (Input_IsActionPressed(ACTION_AIM))
        {
            DrawCrosshair();
        }
        break;
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
