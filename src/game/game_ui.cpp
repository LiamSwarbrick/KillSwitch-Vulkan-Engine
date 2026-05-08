#include "game_ui.h"
#include "core/input.h"
#include "renderer/debug_ui_api.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================
//  Internal state
// ============================================================

static GameState s_state = GameState::MainMenu;

// Font slots — indexed by GameFont enum.
// make sure slot 0 (GameFont::Default) is always nullptr → ImGui built-in default.
static ImFont* s_fonts[static_cast<int>(GameFont::Count)] = {};

enum class MainMenuAnimPhase
{
    TitleReveal,
    AwaitInput,
    RevealMenu,
    MenuReady,
};

struct MainMenuAnimState
{
    MainMenuAnimPhase phase = MainMenuAnimPhase::TitleReveal;
    float phase_time = 0.0f;
    float total_time = 0.0f;
};

static MainMenuAnimState s_main_menu_anim = {};

struct SkillChoiceState
{
    bool active = false;
    int level_index = 0;
    LevelStartSkillOption options[LEVEL_START_SKILL_OPTION_COUNT] = {};
    LevelStartSkillApplyCallback apply_callback = nullptr;
    Scene* scene = nullptr;
};

static SkillChoiceState s_skill_choice = {};

static const LevelStartSkillOption s_default_skill_options[LEVEL_START_SKILL_OPTION_COUNT] = {
    // NOTE(Liam): Deciding to remove the hints about how to use the upgrades so they discover it themselves
    //             We can decide whether to add it back maybe.
    { "skill_placeholder_alpha", "Quick Draw", "Reload barrel 1.25x faster!" },
    { "skill_placeholder_beta",  "Big Fat Gun", "+1 Piering but higher recoil.\nPierce through 1 more zombie with a single bullet." },// Line 'em up in hoards to use ammo wisely!" },
    { "skill_placeholder_gamma", "Juggernaut", "Tank another chomp to the face!\n+1 Health." }// If you run out of bullets you can tank some zombo damage on your way to an ammo box!" },
};

static constexpr float MAIN_MENU_TITLE_REVEAL_SEC = 1.45f;
static constexpr float MAIN_MENU_PROMPT_FADE_SEC  = 0.55f;
static constexpr float MAIN_MENU_REVEAL_SEC       = 0.85f;


//  Font loading
bool GameUI_LoadFont(GameFont slot, const char* ttf_path, float size_pixels)
{
    // GameFont::Default is a reserved no-op slot (always nullptr).
    if (slot == GameFont::Default || slot >= GameFont::Count)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    ImFont* f = io.Fonts->AddFontFromFileTTF(ttf_path, size_pixels);
    if (!f)
    {
        printf("GameUI_LoadFont: failed to load '%s'\n", ttf_path);
        return false;
    }
    s_fonts[static_cast<int>(slot)] = f;
    return true;
}

ImFont* GameUI_GetFont(GameFont slot)
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(GameFont::Count))
        return nullptr;
    return s_fonts[idx]; // nullptr = default font
}

void GameUI_OpenLevelStartSkillSelection(int level_index, const LevelStartSkillOption* options)
{
    s_skill_choice.active = true;
    s_skill_choice.level_index = level_index;

    const LevelStartSkillOption* source = options ? options : s_default_skill_options;
    for (int index = 0; index < LEVEL_START_SKILL_OPTION_COUNT; ++index)
        s_skill_choice.options[index] = source[index];
}

bool GameUI_IsLevelStartSkillSelectionOpen()
{
    return s_skill_choice.active;
}

void GameUI_SetLevelStartSkillApplyCallback(Scene* scene, LevelStartSkillApplyCallback callback)
{
    s_skill_choice.apply_callback = callback;
    s_skill_choice.scene = scene;
}

// ============================================================
//  Helpers — shared style
// ============================================================

// Push a consistent button style: transparent background, white text,
// with a subtle hover highlight.
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

static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float EaseOutCubic(float t)
{
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static float EaseInOutCubic(float t)
{
    t = Clamp01(t);
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float inv = -2.0f * t + 2.0f;
    return 1.0f - (inv * inv * inv) * 0.5f;
}

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static void ResetMainMenuAnimation()
{
    s_main_menu_anim.phase = MainMenuAnimPhase::TitleReveal;
    s_main_menu_anim.phase_time = 0.0f;
    s_main_menu_anim.total_time = 0.0f;
}

static void TriggerMainMenuInteraction()
{
    if (s_main_menu_anim.phase == MainMenuAnimPhase::AwaitInput)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::RevealMenu;
        s_main_menu_anim.phase_time = 0.0f;
    }
}

static void CommitSkillChoice(int selected_index)
{
    if (!s_skill_choice.active)
        return;
    if (selected_index < 0 || selected_index >= LEVEL_START_SKILL_OPTION_COUNT)
        return;

    LevelStartSkillSelection selection = {};
    selection.level_index = s_skill_choice.level_index;
    selection.selected_index = selected_index;
    selection.selected_option = s_skill_choice.options[selected_index];

    if (s_skill_choice.apply_callback && s_skill_choice.scene)
        s_skill_choice.apply_callback(*s_skill_choice.scene, selection);

    s_skill_choice.active = false;
}

static void AdvanceMainMenuAnimation(float dt)
{
    s_main_menu_anim.total_time += dt;
    s_main_menu_anim.phase_time += dt;

    if (s_main_menu_anim.phase == MainMenuAnimPhase::TitleReveal &&
        s_main_menu_anim.phase_time >= MAIN_MENU_TITLE_REVEAL_SEC)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::AwaitInput;
        s_main_menu_anim.phase_time = 0.0f;
    }
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu &&
             s_main_menu_anim.phase_time >= MAIN_MENU_REVEAL_SEC)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::MenuReady;
        s_main_menu_anim.phase_time = 0.0f;
    }
}

static void DrawGradientPromptCentered(ImDrawList* draw_list, const ImVec2& center, float alpha)
{
    if (alpha <= 0.001f)
        return;

    const char* prompt = "PRESS ANY BUTTON";
    ImFont* prompt_font = GameUI_GetFont(GameFont::Body);
    if (!prompt_font)
        prompt_font = ImGui::GetFont();

    float font_size = 28.0f;
    ImVec2 text_size = prompt_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, prompt);
    ImVec2 pos(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f);

    const float breath = 0.35f + 0.65f * (0.5f + 0.5f * sinf(s_main_menu_anim.total_time * 2.0f));
    const float line_alpha = alpha * breath;
    const ImU32 shadow_col = IM_COL32(0, 0, 0, (int)(120.0f * line_alpha));
    const ImU32 text_col = IM_COL32(255, 255, 255, (int)(255.0f * line_alpha));

    size_t char_count = strlen(prompt);
    float x = pos.x;

    for (size_t i = 0; i < char_count; ++i)
    {
        char glyph[2] = { prompt[i], '\0' };
        ImVec2 glyph_size = prompt_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, glyph);

        draw_list->AddText(prompt_font, font_size, ImVec2(x + 1.0f, pos.y + 1.0f), shadow_col, glyph);
        draw_list->AddText(prompt_font, font_size, ImVec2(x, pos.y), text_col, glyph);
        x += glyph_size.x;
    }
}

static bool DrawSkillChoiceCard(int index, const LevelStartSkillOption& option, float card_width, float card_height)
{
    bool chosen = false;

    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::BeginChild("##skill_card", ImVec2(card_width, card_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 32.0f);
        ImGui::TextWrapped("%s", option.display_name ? option.display_name : "Placeholder Skill");
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PopFont();
        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 16.0f);
        ImGui::TextWrapped("%s", option.description ? option.description : "Skill description");

        float button_y = card_height - 54.0f;
        if (ImGui::GetCursorPosY() < button_y)
            ImGui::SetCursorPosY(button_y);

        if (ImGui::Button("Choose", ImVec2(-1.0f, 40.0f)))
            chosen = true;
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(4);
    ImGui::PopID();

    return chosen;
}

static void DrawSkillChoiceModal()
{
    if (!s_skill_choice.active)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 190));

    const float panel_w = (io.DisplaySize.x > 1120.0f) ? 1020.0f : io.DisplaySize.x - 80.0f;
    const float panel_h = (io.DisplaySize.y > 620.0f) ? 430.0f : io.DisplaySize.y - 80.0f;
    const float gap = 18.0f;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.55f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.18f));

    ImGui::Begin("##level_skill_choice", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    char header[96] = {};
    snprintf(header, sizeof(header), "LEVEL %d START", s_skill_choice.level_index);

    ImGui::PushFont(GameUI_GetFont(GameFont::Title), 38.0f);
    ImGui::TextUnformatted(header);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushFont(GameUI_GetFont(GameFont::Title), 30.0f);
    ImGui::TextWrapped("Choose your Hope.");
    ImGui::PopFont();
    ImGui::Spacing();

    float available_w = ImGui::GetContentRegionAvail().x;
    float card_w = (available_w - gap * 2.0f) / 3.0f;
    float card_h = 250.0f;

    for (int index = 0; index < LEVEL_START_SKILL_OPTION_COUNT; ++index)
    {
        if (index > 0)
            ImGui::SameLine(0.0f, gap);

        if (DrawSkillChoiceCard(index, s_skill_choice.options[index], card_w, card_h))
            CommitSkillChoice(index);
    }

    ImGui::Spacing();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ============================================================
//  Main Menu
// ============================================================

static void DrawMainMenu()
{
    ImGuiIO& io = ImGui::GetIO();
    AdvanceMainMenuAnimation(io.DeltaTime);

    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    float title_reveal = 1.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::TitleReveal)
        title_reveal = EaseInOutCubic(s_main_menu_anim.phase_time / MAIN_MENU_TITLE_REVEAL_SEC);

    float title_shift = 0.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu)
        title_shift = EaseInOutCubic(s_main_menu_anim.phase_time / MAIN_MENU_REVEAL_SEC);
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::MenuReady)
        title_shift = 1.0f;

    float prompt_fade = 0.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::AwaitInput)
        prompt_fade = Clamp01(s_main_menu_anim.phase_time / MAIN_MENU_PROMPT_FADE_SEC);
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu)
        prompt_fade = 1.0f - EaseOutCubic(s_main_menu_anim.phase_time / (MAIN_MENU_REVEAL_SEC * 0.55f));

    float menu_reveal = 0.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu)
        menu_reveal = EaseOutCubic(Clamp01((s_main_menu_anim.phase_time - 0.12f) / (MAIN_MENU_REVEAL_SEC - 0.12f)));
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::MenuReady)
        menu_reveal = 1.0f;

    // Full-screen black background
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 255));

    float title_y_center = sh * 0.36f;
    float title_y_top = sh * 0.22f;
    float title_y = Lerp(title_y_center, title_y_top, title_shift);

    // --- Title ---
    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, title_y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, title_reveal);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##title", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(GameUI_GetFont(GameFont::Title), 150.0f);
    ImGui::TextUnformatted("HELL MIST");
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    DrawGradientPromptCentered(dl, ImVec2(sw * 0.5f, title_y + sh * 0.18f), prompt_fade);

    // --- Buttons: centered horizontally, lower half of screen ---
    const float btn_w  = 260.0f;
    const float btn_h  =  48.0f;
    const float gap    =  16.0f;
    const float pad_v  =   2.0f;
    const float total_h = pad_v * 2.0f + btn_h * 3.0f + gap * 2.0f;

    float bx = sw * 0.5f - btn_w * 0.5f;
    float by = sh * 0.5f + (1.0f - menu_reveal) * 26.0f;

    ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btn_w, total_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menu_reveal);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, pad_v));
    ImGui::Begin("##mainmenu_btns", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ((s_main_menu_anim.phase == MainMenuAnimPhase::MenuReady) ? 0 : ImGuiWindowFlags_NoInputs));

    PushMenuButtonStyle();
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 24.0f);

    if (ImGui::Button("Start Game", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Playing);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings", ImVec2(btn_w, btn_h)))
    { /* TODO: open settings panel */ }

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Quitting);

    ImGui::PopFont();
    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
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
    const float panel_h = btn_h * 4 + gap * 3 + pad * 2 + 32.0f; // + title height
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
    ImFont* title_font = GameUI_GetFont(GameFont::Title);
    if (title_font)
        ImGui::PushFont(title_font, 20.0f);
    ImGui::TextUnformatted("PAUSED");
    if (title_font)
        ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    PushMenuButtonStyle();

    if (ImGui::Button("Continue",           ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Playing);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings",           ImVec2(btn_w, btn_h)))
    { /* TODO: open settings panel */ }

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Return to Main Menu", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::MainMenu);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit Game",          ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Quitting);

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
    ResetMainMenuAnimation();
    s_skill_choice.active = false;
    s_skill_choice.scene = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    if (io.FontDefault == nullptr)
        io.FontDefault = io.Fonts->AddFontDefault();

    // Load custom fonts here via GameUI_LoadFont() before the first frame.
    // Example:
    GameUI_LoadFont(GameFont::Title, "assets/UI/title fonts/HelpMe.ttf", 100.0f);
    //   GameUI_LoadFont(GameFont::Body,  "assets/fonts/my_body.ttf",  22.0f);
    //   GameUI_LoadFont(GameFont::Mono,  "assets/fonts/my_mono.ttf",  18.0f);
    // Unloaded slots fall back to the ImGui built-in default font automatically.
}

void GameUI_Update()
{
    // While Debug UI is open, freeze game UI state transitions.
    // This keeps game UI hidden and restores its previous state when debug UI closes.
    if (DebugUI_IsOpen())
        return;

    if (s_state == GameState::Playing && s_skill_choice.active) // When the skill choice modal is open, block all input.
        return;

    // ESC — toggle pause while playing, or resume from pause
    if (Input_IsActionJustPressed(ACTION_PAUSE))
    {
        if (s_state == GameState::Playing)
            GameUI_SetState(GameState::Paused);
        else if (s_state == GameState::Paused)
            GameUI_SetState(GameState::Playing);
    }

    if (s_state == GameState::MainMenu && Input_WasAnyInputJustPressed())
        TriggerMainMenuInteraction();
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
    dl->AddCircleFilled(c, 3.0f, col); // dot crosshair
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
        if (s_skill_choice.active) // draw skill for test
            DrawSkillChoiceModal();
        if (!s_skill_choice.active && Input_IsActionPressed(ACTION_AIM))
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
    if (s_state != GameState::MainMenu && state == GameState::MainMenu)
        ResetMainMenuAnimation();

    if (state != GameState::Playing)
        s_skill_choice.active = false;

    s_state = state;
}
