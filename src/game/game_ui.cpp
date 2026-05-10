#include "game_ui.h"
#include "core/input.h"
#include "foundations/scene.h"
#include "game/foundations/components.h"
#include "renderer/renderer.h"
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
    FadeInMenuButtons,
    MenuReady,
};

struct MainMenuAnimState
{
    MainMenuAnimPhase phase = MainMenuAnimPhase::TitleReveal;
    float phase_time = 0.0f;
    float total_time = 0.0f;
};

static MainMenuAnimState s_main_menu_anim = {};

enum class GameOverAnimPhase
{
    TitleReveal,
    FadeInMenuButtons,
    MenuReady,
};

struct GameOverAnimState
{
    GameOverAnimPhase phase = GameOverAnimPhase::TitleReveal;
    float phase_time = 0.0f;
};

static GameOverAnimState s_game_over_anim = {};

enum class OptionsPanelSection
{
    None,
    Controls,
    Game,
    Graphics,
};

struct OptionsPanelState
{
    bool active = false;
    GameState return_state = GameState::MainMenu;
    OptionsPanelSection selected_section = OptionsPanelSection::None;
    float detail_anim_time = 0.0f;
    bool awaiting_binding = false;
    InputAction awaiting_action = ACTION_COUNT;
    InputBindingDeviceGroup awaiting_device_group = INPUT_BINDING_DEVICE_KEYBOARD_MOUSE;
    bool request_nav_focus = false;
};

static OptionsPanelState s_options_panel = {};

enum class SkillChoicePhase
{
    LevelIntro,
    RevealCards,
    AwaitSelection,
    FadeOut,
};

struct SkillChoiceState
{
    bool active = false;
    int level_index = 0;
    LevelStartSkillOption options[LEVEL_START_SKILL_OPTION_COUNT] = {};
    LevelStartSkillApplyCallback apply_callback = nullptr;
    Scene* scene = nullptr;
    SkillChoicePhase phase = SkillChoicePhase::LevelIntro;
    float phase_time = 0.0f;
    int selected_index = -1;
};

static SkillChoiceState s_skill_choice = {};

struct PlayingHUDState
{
    int life_count = 0;
    int loaded_bullets = 0;
    int backup_bullets = 0;
    float damage_flash_time = 0.0f;
};

struct PlayingHUDResources
{
    Renderer_UITexture heart_icon = {};
    Renderer_UITexture layer_banner = {};
    Renderer_UITexture damage_overlay = {};
};

static PlayingHUDState s_playing_hud = {};
static PlayingHUDResources s_playing_hud_resources = {};

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
static constexpr float MAIN_MENU_BUTTON_FADE_SEC  = 0.45f;
static constexpr float GAME_OVER_TITLE_REVEAL_SEC = 0.90f;
static constexpr float GAME_OVER_MENU_FADE_SEC    = 0.40f;
static constexpr float OPTIONS_DETAIL_REVEAL_SEC  = 0.25f;
static constexpr float SKILL_CHOICE_LEVEL_INTRO_FADE_IN_SEC  = 1.0f;
static constexpr float SKILL_CHOICE_LEVEL_INTRO_HOLD_SEC     = 1.0f;
static constexpr float SKILL_CHOICE_LEVEL_INTRO_FADE_OUT_SEC = 0.6f;
static constexpr float SKILL_CHOICE_CARDS_FADE_SEC           = 0.30f;
static constexpr float SKILL_CHOICE_CLOSE_FADE_SEC           = 0.30f;
static constexpr float PLAYING_HUD_DAMAGE_FLASH_SEC          = 0.35f;
static constexpr const char* INPUT_BINDINGS_PATH             = "assets/keybindings.json";
static constexpr const char* HUD_HEART_ICON_PATH             = "assets/UI/ingame png/icon_heart.png";
static constexpr const char* HUD_LAYER_BANNER_PATH           = "assets/UI/ingame png/Layer 46.png";
static constexpr const char* HUD_DAMAGE_OVERLAY_PATH         = "assets/UI/ingame png/LowHealthScreenEffect.png";

struct ControlsActionEntry
{
    InputAction action;
    const char* label;
};

static const ControlsActionEntry s_controls_actions[] = {
    { ACTION_MOVE_FORWARD,  "Move Forward" },
    { ACTION_MOVE_BACKWARD, "Move Backward" },
    { ACTION_MOVE_LEFT,     "Move Left" },
    { ACTION_MOVE_RIGHT,    "Move Right" },
    { ACTION_SPRINT,        "Sprint" },
    { ACTION_JUMP,          "Jump" },
    { ACTION_CROUCH,        "Crouch" },
    { ACTION_CAMERA_UP,     "Camera Up" },
    { ACTION_CAMERA_DOWN,   "Camera Down" },
    { ACTION_CAMERA_LEFT,   "Camera Left" },
    { ACTION_CAMERA_RIGHT,  "Camera Right" },
    { ACTION_TOGGLE_CAMERA, "Toggle Camera" },
    { ACTION_INTERACT,      "Interact" },
    { ACTION_ATTACK,        "Attack" },
    { ACTION_AIM,           "Aim" },
    { ACTION_PAUSE,         "Pause" },
};

static EntityID FindFirstPlayerEntity(Scene& scene)
{
    EntityID player_id = NULL_ENTITY;
    scene.GetECS().GetView<C_PlayerInput>().ForEach([&](EntityID id, C_PlayerInput&)
    {
        if (player_id == NULL_ENTITY)
            player_id = id;
    });

    return player_id;
}

static C_Health* TryGetPlayerHealth(Scene& scene, EntityID player_id)
{
    ECS& ecs = scene.GetECS();
    if (!ecs.IsEntityValid(player_id) || !ecs.Has<C_Health>(player_id))
        return nullptr;

    return &ecs.GetComponent<C_Health>(player_id);
}

static GameUIPlayingHUDState BuildPlayingHUDState(Scene& scene, EntityID player_id, bool* out_has_real_health)
{
    GameUIPlayingHUDState hud_state = {};
    bool has_real_health = false;

    if (const C_Health* health = TryGetPlayerHealth(scene, player_id))
    {
        hud_state.life_count = (health->currentHealth < 0) ? 0 : health->currentHealth;
        has_real_health = true;
    }

    ECS& ecs = scene.GetECS();
    if (ecs.IsEntityValid(player_id) && ecs.Has<C_WeaponSocket>(player_id))
    {
        const C_WeaponSocket& weapon_socket = ecs.GetComponent<C_WeaponSocket>(player_id);
        const bool has_ranged_weapon = weapon_socket.weapon_entity != NULL_ENTITY &&
            ecs.IsEntityValid(weapon_socket.weapon_entity) &&
            ecs.Has<C_WeaponRanged>(weapon_socket.weapon_entity);

        if (has_ranged_weapon)
        {
            const C_WeaponRanged& weapon = ecs.GetComponent<C_WeaponRanged>(weapon_socket.weapon_entity);
            hud_state.loaded_bullets = weapon.currentBullets;
            hud_state.backup_bullets = 0;
        }
    }

    if (out_has_real_health)
        *out_has_real_health = has_real_health;

    return hud_state;
}


//  Font loading
bool GameUI_LoadFont(GameFont slot, const char* ttf_path, float size_pixels)
{
    // GameFont::Default is a reserved no-op slot (always nullptr).
    if (slot == GameFont::Default || slot >= GameFont::Count)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    if (io.FontDefault == nullptr)
        io.FontDefault = io.Fonts->AddFontDefault();

    ImFont* f = io.Fonts->AddFontFromFileTTF(ttf_path, size_pixels);
    if (!f)
    {
        printf("GameUI_LoadFont: failed to load '%s'\n", ttf_path);
        return false;
    }

    ImFontConfig fallback_config;
    fallback_config.MergeMode = true;
    fallback_config.DstFont = f;
    io.Fonts->AddFontDefault(&fallback_config);

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
    s_skill_choice.phase = SkillChoicePhase::LevelIntro;
    s_skill_choice.phase_time = 0.0f;
    s_skill_choice.selected_index = -1;

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

void GameUI_ApplyPlaceholderLevelStartSkill(Scene& scene, const LevelStartSkillSelection& selection)
{
    SDL_Log(
        "GameUI: selected level-start skill [level=%d, slot=%d, id=%s, name=%s]",
        selection.level_index,
        selection.selected_index,
        selection.selected_option.skill_id ? selection.selected_option.skill_id : "<null>",
        selection.selected_option.display_name ? selection.selected_option.display_name : "<null>");

    if (selection.selected_option.skill_id != nullptr &&
        SDL_strcmp(selection.selected_option.skill_id, "skill_placeholder_gamma") == 0)
    {
        const EntityID player_id = FindFirstPlayerEntity(scene);
        if (C_Health* health = TryGetPlayerHealth(scene, player_id))
        {
            ++health->maxHealth;
            ++health->currentHealth;
        }
    }

    // TODO: Apply real gameplay modifications here.
    // Suggested future integrations:
    // - mutate gameplay components through scene.GetECS()
    // - drive presentation through PlayAnim / PlayUpperBodyAnim on animated entities
    // - store persistent run modifiers in scene-owned gameplay state
}

static void GameUI_SetPlayingHUDState(const GameUIPlayingHUDState& state)
{
    const int clamped_life_count = (state.life_count < 0) ? 0 : state.life_count;
    if (clamped_life_count < s_playing_hud.life_count)
        s_playing_hud.damage_flash_time = PLAYING_HUD_DAMAGE_FLASH_SEC;

    s_playing_hud.life_count = clamped_life_count;
    s_playing_hud.loaded_bullets = (state.loaded_bullets < 0) ? 0 : state.loaded_bullets;
    s_playing_hud.backup_bullets = (state.backup_bullets < 0) ? 0 : state.backup_bullets;
}

void GameUI_UpdatePlayingHUD(Scene& scene)
{
    bool has_real_health = false;
    const EntityID player_id = FindFirstPlayerEntity(scene);
    const GameUIPlayingHUDState hud_state = BuildPlayingHUDState(scene, player_id, &has_real_health);

    GameUI_SetPlayingHUDState(hud_state);

    if (s_state == GameState::Playing && has_real_health && hud_state.life_count <= 0)
        GameUI_SetState(GameState::GameOver);
}

void GameUI_DebugDamagePlayer(Scene& scene, int damage_amount)
{
    if (damage_amount <= 0)
        return;

    const EntityID player_id = FindFirstPlayerEntity(scene);
    C_Health* health = TryGetPlayerHealth(scene, player_id);
    if (!health)
        return;

    health->currentHealth -= damage_amount;
    if (health->currentHealth < 0)
        health->currentHealth = 0;
}

void GameUI_TriggerDamageFlash()
{
    s_playing_hud.damage_flash_time = PLAYING_HUD_DAMAGE_FLASH_SEC;
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

static ImVec2 GetTextureSizeForHeight(const Renderer_UITexture& texture, float target_height)
{
    if (texture.width == 0 || texture.height == 0 || target_height <= 0.0f)
        return ImVec2(0.0f, 0.0f);

    const float aspect = static_cast<float>(texture.width) / static_cast<float>(texture.height);
    return ImVec2(target_height * aspect, target_height);
}

static void DrawPlayingHUD()
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float scale = io.DisplaySize.y / 1080.0f; 
    if (scale < 0.75f) scale = 0.75f;
    if (scale > 1.35f) scale = 1.35f;

    ImFont* hud_font = GameUI_GetFont(GameFont::Body);
    if (!hud_font)
        hud_font = ImGui::GetFont();

    const float margin_x = 34.0f * scale;
    const float margin_y = 28.0f * scale;
    const float banner_height = 82.0f * scale;
    const float row_gap = 14.0f * scale;
    const float heart_height = 32.0f * scale;
    const float heart_gap = 8.0f * scale;
    const float ammo_gap = 18.0f * scale;
    const float ammo_font_size = 30.0f * scale;

    const ImVec2 banner_size = GetTextureSizeForHeight(s_playing_hud_resources.layer_banner, banner_height);

    char ammo_label[32] = {};
    snprintf(ammo_label, sizeof(ammo_label), "%d / %d", s_playing_hud.loaded_bullets, s_playing_hud.backup_bullets);
    

    const ImVec2 ammo_text_size = hud_font->CalcTextSizeA(ammo_font_size, FLT_MAX, 0.0f, ammo_label);
    const float top_row_width = banner_size.x + ammo_gap + ammo_text_size.x;
    const float top_row_right = io.DisplaySize.x - margin_x;
    const float hearts_right = top_row_right;
    const float bottom_anchor = io.DisplaySize.y - margin_y;
    const float hearts_top = bottom_anchor - heart_height;
    const float row_top = hearts_top - row_gap - banner_height;
    const ImVec2 banner_pos(top_row_right - top_row_width, row_top);

    if (s_playing_hud_resources.layer_banner.imgui_texture_id != 0)
    {
        dl->AddImage(
            (ImTextureID)(s_playing_hud_resources.layer_banner.imgui_texture_id),
            banner_pos,
            ImVec2(banner_pos.x + banner_size.x, banner_pos.y + banner_size.y));
    }

    const ImVec2 ammo_pos(
        banner_pos.x + banner_size.x + ammo_gap,
        banner_pos.y + (banner_height - ammo_text_size.y) * 0.5f);
    const ImU32 ammo_shadow_col = IM_COL32(0, 0, 0, 180);
    const ImU32 ammo_text_col = IM_COL32(255, 255, 255, 255);
    dl->AddText(hud_font, ammo_font_size, ImVec2(ammo_pos.x + 2.0f, ammo_pos.y + 2.0f), ammo_shadow_col, ammo_label);
    dl->AddText(hud_font, ammo_font_size, ammo_pos, ammo_text_col, ammo_label);

    if (s_playing_hud_resources.heart_icon.imgui_texture_id != 0 && s_playing_hud.life_count > 0)
    {
        const ImVec2 heart_size = GetTextureSizeForHeight(s_playing_hud_resources.heart_icon, heart_height);

        for (int index = 0; index < s_playing_hud.life_count; ++index)
        {
            const float x = hearts_right - heart_size.x * static_cast<float>(index + 1) - heart_gap * static_cast<float>(index);
            dl->AddImage(
                (ImTextureID)(s_playing_hud_resources.heart_icon.imgui_texture_id),
                ImVec2(x, hearts_top),
                ImVec2(x + heart_size.x, hearts_top + heart_size.y));
        }
    }

    if (s_playing_hud.damage_flash_time > 0.0f && s_playing_hud_resources.damage_overlay.imgui_texture_id != 0)
    {
        s_playing_hud.damage_flash_time -= io.DeltaTime;
        if (s_playing_hud.damage_flash_time < 0.0f)
            s_playing_hud.damage_flash_time = 0.0f;

        const float flash_alpha = EaseOutCubic(s_playing_hud.damage_flash_time / PLAYING_HUD_DAMAGE_FLASH_SEC) * 0.78f;
        dl->AddImage(
            (ImTextureID)(s_playing_hud_resources.damage_overlay.imgui_texture_id),
            ImVec2(0.0f, 0.0f),
            io.DisplaySize,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32(255, 255, 255, static_cast<int>(255.0f * flash_alpha)));
    }
}

static bool IsBindingInDeviceGroup(const InputBinding& binding, InputBindingDeviceGroup device_group)
{
    if (device_group == INPUT_BINDING_DEVICE_KEYBOARD_MOUSE)
        return binding.source == BIND_KEYBOARD || binding.source == BIND_MOUSE_BUTTON;

    if (device_group == INPUT_BINDING_DEVICE_GAMEPAD)
        return binding.source == BIND_GAMEPAD_BUTTON ||
               binding.source == BIND_GAMEPAD_AXIS_POS ||
               binding.source == BIND_GAMEPAD_AXIS_NEG;

    return false;
}

static const char* GetControlsActionLabel(InputAction action)
{
    for (const ControlsActionEntry& entry : s_controls_actions)
    {
        if (entry.action == action)
            return entry.label;
    }
    return Input_GetActionName(action);
}

static const char* GetBindingDeviceGroupLabel(InputBindingDeviceGroup device_group)
{
    return device_group == INPUT_BINDING_DEVICE_GAMEPAD ? "gamepad" : "keyboard or mouse";
}

static bool TryGetBindingForDeviceGroup(InputAction action, InputBindingDeviceGroup device_group, InputBinding* out_binding, int* out_slot)
{
    const int binding_count = Input_GetBindingCount(action);
    for (int slot = 0; slot < binding_count; ++slot)
    {
        const InputBinding binding = Input_GetBinding(action, slot);
        if (!IsBindingInDeviceGroup(binding, device_group))
            continue;

        if (out_binding)
            *out_binding = binding;
        if (out_slot)
            *out_slot = slot;
        return true;
    }
    return false;
}

static bool IsOptionsBackJustPressed()
{
    return Input_IsGamepadButtonJustPressed(SDL_GAMEPAD_BUTTON_EAST);
}

static bool IsSkillChoiceMoveLeftJustPressed()
{
    return Input_IsKeyJustPressed(SDL_SCANCODE_LEFT) ||
           Input_IsGamepadButtonJustPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
}

static bool IsSkillChoiceMoveRightJustPressed()
{
    return Input_IsKeyJustPressed(SDL_SCANCODE_RIGHT) ||
           Input_IsGamepadButtonJustPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
}

static bool IsSkillChoiceConfirmJustPressed()
{
    return Input_IsKeyJustPressed(SDL_SCANCODE_RETURN) ||
           Input_IsKeyJustPressed(SDL_SCANCODE_SPACE) ||
           Input_IsGamepadButtonJustPressed(SDL_GAMEPAD_BUTTON_SOUTH);
}

static void CancelControlsBindingCapture()
{
    s_options_panel.awaiting_binding = false;
    s_options_panel.awaiting_action = ACTION_COUNT;
    s_options_panel.awaiting_device_group = INPUT_BINDING_DEVICE_KEYBOARD_MOUSE;
    Input_ClearPendingBindingCapture();
}

static void BeginControlsBindingCapture(InputAction action, InputBindingDeviceGroup device_group)
{
    s_options_panel.awaiting_binding = true;
    s_options_panel.awaiting_action = action;
    s_options_panel.awaiting_device_group = device_group;
    Input_ClearPendingBindingCapture();
}

static void ApplyControlsBinding(InputAction action, InputBindingDeviceGroup device_group, const InputBinding& binding)
{
    int slot = -1;
    if (!TryGetBindingForDeviceGroup(action, device_group, nullptr, &slot))
    {
        const int binding_count = Input_GetBindingCount(action);
        slot = (binding_count < INPUT_MAX_BINDINGS_PER_ACTION) ? binding_count : (INPUT_MAX_BINDINGS_PER_ACTION - 1);
    }

    if (slot < 0)
        return;

    Input_SetBinding(action, slot, binding);
    Input_SaveBindings(INPUT_BINDINGS_PATH);
}

static void DrawControlsBindingCell(InputAction action, InputBindingDeviceGroup device_group)
{
    const bool is_waiting = s_options_panel.awaiting_binding &&
                            s_options_panel.awaiting_action == action &&
                            s_options_panel.awaiting_device_group == device_group;

    InputBinding binding = {};
    const bool has_binding = TryGetBindingForDeviceGroup(action, device_group, &binding, nullptr);

    char label[128] = {};
    if (is_waiting)
        snprintf(label, sizeof(label), "Press input...");
    else if (has_binding)
        snprintf(label, sizeof(label), "%s", Input_GetBindingDisplayName(binding));
    else
        snprintf(label, sizeof(label), "Unbound");

    ImGui::PushID(static_cast<int>(action) * 8 + static_cast<int>(device_group));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, is_waiting ? ImVec4(1.0f, 1.0f, 1.0f, 0.10f) : ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_Border, is_waiting ? ImVec4(1.0f, 1.0f, 1.0f, 0.52f) : ImVec4(1.0f, 1.0f, 1.0f, 0.18f));

    if (ImGui::Button(label, ImVec2(-FLT_MIN, 32.0f)))
        BeginControlsBindingCapture(action, device_group);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

static void DrawControlsDetailPanel()
{
    bool cancel_rebind = false;
    bool cancel_button_activated = false;

    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 18.0f);

    if (s_options_panel.awaiting_binding)
    {
        ImGui::TextWrapped("Rebinding %s. Press a %s input.",
            GetControlsActionLabel(s_options_panel.awaiting_action),
            GetBindingDeviceGroupLabel(s_options_panel.awaiting_device_group));

        PushMenuButtonStyle();
        cancel_rebind = ImGui::Button("Cancel Rebind", ImVec2(180.0f, 32.0f));
        cancel_button_activated = ImGui::IsItemActivated();
        PopMenuButtonStyle();
    }
    ImGui::PopFont();

    if (s_options_panel.awaiting_binding)
    {
        if (s_options_panel.awaiting_device_group == INPUT_BINDING_DEVICE_KEYBOARD_MOUSE && cancel_button_activated)
            Input_ClearPendingBindingCapture();

        if (cancel_rebind)
        {
            CancelControlsBindingCapture();
        }
        else
        {
            InputBinding captured_binding = {};
            if (Input_PollPendingBinding(s_options_panel.awaiting_device_group, &captured_binding))
            {
                ApplyControlsBinding(s_options_panel.awaiting_action, s_options_panel.awaiting_device_group, captured_binding);
                CancelControlsBindingCapture();
            }
        }
    }

    ImGui::Spacing();

    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    const float table_height = ImGui::GetContentRegionAvail().y - 4.0f;
    if (ImGui::BeginTable("##controls_bindings", 3, table_flags, ImVec2(0.0f, table_height)))
    {
        ImGui::TableSetupColumn("##action", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("##keyboard_mouse", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("##gamepad", ImGuiTableColumnFlags_WidthStretch, 0.30f);

        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 20.0f);
        for (const ControlsActionEntry& entry : s_controls_actions)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.label);

            ImGui::TableSetColumnIndex(1);
            DrawControlsBindingCell(entry.action, INPUT_BINDING_DEVICE_KEYBOARD_MOUSE);

            ImGui::TableSetColumnIndex(2);
            DrawControlsBindingCell(entry.action, INPUT_BINDING_DEVICE_GAMEPAD);
        }
        ImGui::PopFont();
        ImGui::EndTable();
    }
}

static void OpenOptionsPanel(GameState return_state)
{
    s_options_panel.active = true;
    s_options_panel.return_state = return_state;
    s_options_panel.selected_section = OptionsPanelSection::None;
    s_options_panel.detail_anim_time = 0.0f;
    s_options_panel.request_nav_focus = true;
    CancelControlsBindingCapture();
}

static void CloseOptionsPanel()
{
    s_options_panel.active = false;
    s_options_panel.selected_section = OptionsPanelSection::None;
    s_options_panel.detail_anim_time = 0.0f;
    s_options_panel.request_nav_focus = false;
    CancelControlsBindingCapture();
}

static void SelectOptionsPanelSection(OptionsPanelSection section)
{
    s_options_panel.selected_section = section;
    s_options_panel.detail_anim_time = 0.0f;
    CancelControlsBindingCapture();
}

static void NavigateOptionsBack()
{
    if (s_options_panel.awaiting_binding)
    {
        CancelControlsBindingCapture();
        return;
    }

    if (s_options_panel.selected_section != OptionsPanelSection::None)
    {
        s_options_panel.selected_section = OptionsPanelSection::None;
        s_options_panel.detail_anim_time = 0.0f;
        s_options_panel.request_nav_focus = true;
        return;
    }

    CloseOptionsPanel();
}

static const char* GetOptionsSectionTitle(OptionsPanelSection section)
{
    switch (section)
    {
    case OptionsPanelSection::Controls: return "Controls";
    case OptionsPanelSection::Game:     return "Game";
    case OptionsPanelSection::Graphics: return "Graphics";
    default:                            return "";
    }
}

static void ResetMainMenuAnimation()
{
    s_main_menu_anim.phase = MainMenuAnimPhase::TitleReveal;
    s_main_menu_anim.phase_time = 0.0f;
    s_main_menu_anim.total_time = 0.0f;
}

static void ResetGameOverAnimation()
{
    s_game_over_anim.phase = GameOverAnimPhase::TitleReveal;
    s_game_over_anim.phase_time = 0.0f;
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
    if (s_skill_choice.phase != SkillChoicePhase::AwaitSelection)
        return;
    if (selected_index < 0 || selected_index >= LEVEL_START_SKILL_OPTION_COUNT)
        return;

    LevelStartSkillSelection selection = {};
    selection.level_index = s_skill_choice.level_index;
    selection.selected_index = selected_index;
    selection.selected_option = s_skill_choice.options[selected_index];

    s_skill_choice.selected_index = selected_index;

    if (s_skill_choice.apply_callback && s_skill_choice.scene)
        s_skill_choice.apply_callback(*s_skill_choice.scene, selection);

    s_skill_choice.phase = SkillChoicePhase::FadeOut;
    s_skill_choice.phase_time = 0.0f;
}

static void AdvanceSkillChoiceAnimation(float dt)
{
    if (!s_skill_choice.active)
        return;

    if (Input_IsActionJustPressed(ACTION_PAUSE))
    {
        s_skill_choice.phase_time = 1000.0f;
    }

    s_skill_choice.phase_time += dt;

    if (s_skill_choice.phase == SkillChoicePhase::LevelIntro)
    {
        const float intro_total =
            SKILL_CHOICE_LEVEL_INTRO_FADE_IN_SEC +
            SKILL_CHOICE_LEVEL_INTRO_HOLD_SEC +
            SKILL_CHOICE_LEVEL_INTRO_FADE_OUT_SEC;

        if (s_skill_choice.phase_time >= intro_total)
        {
            s_skill_choice.phase = SkillChoicePhase::RevealCards;
            s_skill_choice.phase_time = 0.0f;
        }
    }
    else if (s_skill_choice.phase == SkillChoicePhase::RevealCards)
    {
        if (s_skill_choice.phase_time >= SKILL_CHOICE_CARDS_FADE_SEC)
        {
            s_skill_choice.phase = SkillChoicePhase::AwaitSelection;
            s_skill_choice.phase_time = 0.0f;
            s_skill_choice.selected_index = -1;
        }
    }
    else if (s_skill_choice.phase == SkillChoicePhase::FadeOut)
    {
        if (s_skill_choice.phase_time >= SKILL_CHOICE_CLOSE_FADE_SEC)
        {
            s_skill_choice.active = false;
            s_skill_choice.phase = SkillChoicePhase::LevelIntro;
            s_skill_choice.phase_time = 0.0f;
            s_skill_choice.selected_index = -1;
        }
    }
}

static void AdvanceMainMenuAnimation(float dt)
{
    s_main_menu_anim.total_time += dt;
    s_main_menu_anim.phase_time += dt;

    // Skip menu pressing escape (aka PAUSE)
    if (Input_IsActionJustPressed(ACTION_PAUSE))
    {
        s_main_menu_anim.phase_time = 1000.0f;
    }

    if (s_main_menu_anim.phase == MainMenuAnimPhase::TitleReveal &&
        s_main_menu_anim.phase_time >= MAIN_MENU_TITLE_REVEAL_SEC)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::AwaitInput;
        s_main_menu_anim.phase_time = 0.0f;
    }
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu &&
             s_main_menu_anim.phase_time >= MAIN_MENU_REVEAL_SEC)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::FadeInMenuButtons;
        s_main_menu_anim.phase_time = 0.0f;
    }
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::FadeInMenuButtons &&
             s_main_menu_anim.phase_time >= MAIN_MENU_BUTTON_FADE_SEC)
    {
        s_main_menu_anim.phase = MainMenuAnimPhase::MenuReady;
        s_main_menu_anim.phase_time = 0.0f;
    }
}

static void AdvanceGameOverAnimation(float dt)
{
    s_game_over_anim.phase_time += dt;

    if (s_game_over_anim.phase == GameOverAnimPhase::TitleReveal &&
        s_game_over_anim.phase_time >= GAME_OVER_TITLE_REVEAL_SEC)
    {
        s_game_over_anim.phase = GameOverAnimPhase::FadeInMenuButtons;
        s_game_over_anim.phase_time = 0.0f;
    }
    else if (s_game_over_anim.phase == GameOverAnimPhase::FadeInMenuButtons &&
             s_game_over_anim.phase_time >= GAME_OVER_MENU_FADE_SEC)
    {
        s_game_over_anim.phase = GameOverAnimPhase::MenuReady;
        s_game_over_anim.phase_time = 0.0f;
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

static void DrawOptionsPanel()
{
    if (!s_options_panel.active)
        return;

    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    if (s_options_panel.selected_section != OptionsPanelSection::None)
    {
        s_options_panel.detail_anim_time += io.DeltaTime;
        if (s_options_panel.detail_anim_time > OPTIONS_DETAIL_REVEAL_SEC)
            s_options_panel.detail_anim_time = OPTIONS_DETAIL_REVEAL_SEC;
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 255));

    ImGui::SetNextWindowPos(ImVec2(28.0f, 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(140.0f, 60.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##options_back", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    PushMenuButtonStyle();
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 32.0f);
    if (ImGui::Button("Back", ImVec2(120.0f, 40.0f)))
        CloseOptionsPanel();
    ImGui::PopFont();
    PopMenuButtonStyle();
    ImGui::Dummy(ImVec2(0, 12.0f));


    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowPos(ImVec2(sw * 0.12f, sh * 0.26f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260.0f, 250.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##options_nav", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    
    ImGui::Spacing();
    ImGui::Spacing();

    PushMenuButtonStyle();
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 26.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

    if (ImGui::Button("Controls", ImVec2(220.0f, 44.0f)))
        SelectOptionsPanelSection(OptionsPanelSection::Controls);
    if (s_options_panel.request_nav_focus)
    {
        ImGui::SetItemDefaultFocus();
        s_options_panel.request_nav_focus = false;
    }

    ImGui::Dummy(ImVec2(0, 12.0f));

    if (ImGui::Button("Game", ImVec2(220.0f, 44.0f)))
        SelectOptionsPanelSection(OptionsPanelSection::Game);

    ImGui::Dummy(ImVec2(0, 12.0f));

    if (ImGui::Button("Graphics", ImVec2(220.0f, 44.0f)))
        SelectOptionsPanelSection(OptionsPanelSection::Graphics);
    ImGui::PopStyleVar();

    ImGui::PopFont();
    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (s_options_panel.selected_section == OptionsPanelSection::None)
        return;

    const float panel_x = sw * 0.42f;
    const float panel_y = sh * 0.15f;
    const float panel_w = sw * 0.50f;
    const float panel_h = sh * 0.70f;
    const float detail_reveal = EaseOutCubic(s_options_panel.detail_anim_time / OPTIONS_DETAIL_REVEAL_SEC);
    const float detail_x = panel_x - (1.0f - detail_reveal) * 48.0f;

    ImGui::SetNextWindowPos(ImVec2(detail_x, panel_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, detail_reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.18f));
    ImGui::Begin("##options_detail", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 34.0f);
    ImGui::TextUnformatted(GetOptionsSectionTitle(s_options_panel.selected_section));
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Spacing();

    if (s_options_panel.selected_section == OptionsPanelSection::Controls)
    {
        DrawControlsDetailPanel();
    }
    else
    {
        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 22.0f);
        ImGui::TextWrapped("Placeholder.");
        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

static bool DrawSkillChoiceCard(int index, const LevelStartSkillOption& option, float card_width, float card_height, bool allow_select)
{
    bool chosen = false;
    const bool is_selected = s_skill_choice.selected_index == index;

    ImGui::PushID(index);

    const ImVec2 card_min = ImGui::GetCursorScreenPos();
    const ImVec2 card_max(card_min.x + card_width, card_min.y + card_height);
    const bool is_hovered = allow_select && ImGui::IsMouseHoveringRect(card_min, card_max, false);
    const float card_fill_alpha = is_selected ? 0.14f : (is_hovered ? 0.08f : 0.0f);
    const float card_border_alpha = is_selected ? 0.82f : (is_hovered ? 0.46f : 0.18f);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(card_fill_alpha, card_fill_alpha, card_fill_alpha, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, card_border_alpha));

    if (ImGui::BeginChild("##skill_card", ImVec2(card_width, card_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 32.0f);
        ImGui::TextWrapped("%s", option.display_name ? option.display_name : "Placeholder Skill");
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushFont(GameUI_GetFont(GameFont::Body), 24.0f);
        ImGui::TextWrapped("%s", option.description ? option.description : "Skill description");
        ImGui::PopFont();
    }
    ImGui::EndChild();

    if (is_hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    if (allow_select && ImGui::IsItemClicked(ImGuiMouseButton_Left))
        chosen = true;

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopID();

    return chosen;
}

static void DrawSkillChoiceModal()
{
    if (!s_skill_choice.active)
        return;

    ImGuiIO& io = ImGui::GetIO();
    AdvanceSkillChoiceAnimation(io.DeltaTime);

    if (!s_skill_choice.active)
        return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float intro_alpha = 0.0f;
    float cards_alpha = 0.0f;
    if (s_skill_choice.phase == SkillChoicePhase::LevelIntro)
    {
        const float fade_in_end = SKILL_CHOICE_LEVEL_INTRO_FADE_IN_SEC;
        const float hold_end = fade_in_end + SKILL_CHOICE_LEVEL_INTRO_HOLD_SEC;

        if (s_skill_choice.phase_time < fade_in_end)
            intro_alpha = EaseOutCubic(s_skill_choice.phase_time / SKILL_CHOICE_LEVEL_INTRO_FADE_IN_SEC);
        else if (s_skill_choice.phase_time < hold_end)
            intro_alpha = 1.0f;
        else
            intro_alpha = 1.0f - EaseInOutCubic((s_skill_choice.phase_time - hold_end) / SKILL_CHOICE_LEVEL_INTRO_FADE_OUT_SEC);
    }
    else if (s_skill_choice.phase == SkillChoicePhase::RevealCards)
    {
        cards_alpha = EaseOutCubic(s_skill_choice.phase_time / SKILL_CHOICE_CARDS_FADE_SEC);
    }
    else if (s_skill_choice.phase == SkillChoicePhase::AwaitSelection)
    {
        cards_alpha = 1.0f;
    }
    else if (s_skill_choice.phase == SkillChoicePhase::FadeOut)
    {
        cards_alpha = 1.0f - EaseInOutCubic(s_skill_choice.phase_time / SKILL_CHOICE_CLOSE_FADE_SEC);
    }

    const float backdrop_alpha =
        (s_skill_choice.phase == SkillChoicePhase::LevelIntro) ? 220.0f : (190.0f * cards_alpha);
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 255));

    if (s_skill_choice.phase == SkillChoicePhase::LevelIntro)
    {
        char header[96] = {};
        snprintf(header, sizeof(header), "LEVEL %d START", s_skill_choice.level_index);

        ImFont* intro_font = GameUI_GetFont(GameFont::Title);
        if (!intro_font)
            intro_font = ImGui::GetFont();

        const float font_size = 68.0f;
        const ImVec2 text_size = intro_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, header);
        const ImVec2 text_pos((io.DisplaySize.x - text_size.x) * 0.5f, (io.DisplaySize.y - text_size.y) * 0.5f);
        const ImU32 shadow_col = IM_COL32(0, 0, 0, (int)(140.0f * intro_alpha));
        const ImU32 text_col = IM_COL32(255, 255, 255, (int)(255.0f * intro_alpha));

        dl->AddText(intro_font, font_size, ImVec2(text_pos.x + 2.0f, text_pos.y + 2.0f), shadow_col, header);
        dl->AddText(intro_font, font_size, text_pos, text_col, header);
        return;
    }

    const float panel_w = (io.DisplaySize.x > 1120.0f) ? 1020.0f : io.DisplaySize.x - 80.0f;
    const float panel_h = (io.DisplaySize.y > 620.0f) ? 430.0f : io.DisplaySize.y - 80.0f;
    const float gap = 18.0f;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, cards_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    ImGui::Begin("##level_skill_choice", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(GameUI_GetFont(GameFont::Title), 45.0f);
    const char* title = "Choose your Fate.";  // NOTE: Changing from hope to pretend that it's a curse or some shit lol
    const float title_offset = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(title).x) * 0.5f;
    if (title_offset > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + title_offset);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::Spacing();

    float available_w = ImGui::GetContentRegionAvail().x;
    float card_w = (available_w - gap * 2.0f) / 3.0f;
    float card_h = 250.0f;
    const bool allow_select = s_skill_choice.phase == SkillChoicePhase::AwaitSelection;

    if (allow_select)
    {
        if (Input_IsGamepadConnected() &&
            (s_skill_choice.selected_index < 0 || s_skill_choice.selected_index >= LEVEL_START_SKILL_OPTION_COUNT))
            s_skill_choice.selected_index = 0;

        if (IsSkillChoiceMoveLeftJustPressed())
        {
            if (s_skill_choice.selected_index < 0 || s_skill_choice.selected_index >= LEVEL_START_SKILL_OPTION_COUNT)
                s_skill_choice.selected_index = LEVEL_START_SKILL_OPTION_COUNT - 1;
            else
                s_skill_choice.selected_index = (s_skill_choice.selected_index + LEVEL_START_SKILL_OPTION_COUNT - 1) % LEVEL_START_SKILL_OPTION_COUNT;
        }
        else if (IsSkillChoiceMoveRightJustPressed())
        {
            if (s_skill_choice.selected_index < 0 || s_skill_choice.selected_index >= LEVEL_START_SKILL_OPTION_COUNT)
                s_skill_choice.selected_index = 0;
            else
                s_skill_choice.selected_index = (s_skill_choice.selected_index + 1) % LEVEL_START_SKILL_OPTION_COUNT;
        }

        if (IsSkillChoiceConfirmJustPressed())
            CommitSkillChoice(s_skill_choice.selected_index);
    }

    for (int index = 0; index < LEVEL_START_SKILL_OPTION_COUNT; ++index)
    {
        if (index > 0)
            ImGui::SameLine(0.0f, gap);

        if (DrawSkillChoiceCard(index, s_skill_choice.options[index], card_w, card_h, allow_select))
            CommitSkillChoice(index);
    }

    ImGui::Spacing();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
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
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::FadeInMenuButtons)
        title_shift = 1.0f;
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::MenuReady)
        title_shift = 1.0f;

    float prompt_fade = 0.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::AwaitInput)
        prompt_fade = Clamp01(s_main_menu_anim.phase_time / MAIN_MENU_PROMPT_FADE_SEC);
    else if (s_main_menu_anim.phase == MainMenuAnimPhase::RevealMenu)
        prompt_fade = 1.0f - EaseOutCubic(s_main_menu_anim.phase_time / (MAIN_MENU_REVEAL_SEC * 0.55f));

    float menu_reveal = 0.0f;
    if (s_main_menu_anim.phase == MainMenuAnimPhase::FadeInMenuButtons)
        menu_reveal = EaseInOutCubic(s_main_menu_anim.phase_time / MAIN_MENU_BUTTON_FADE_SEC);
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
    const float total_h = pad_v * 2.0f + btn_h * 3.0f + gap * 3.0f ;

    float bx = sw * 0.5f - btn_w * 0.5f;
    float by = sh * 0.5f;

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
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 30.0f);

    if (ImGui::Button("Start Game", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Playing);
    ImGui::SetItemDefaultFocus();

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings", ImVec2(btn_w, btn_h)))
        OpenOptionsPanel(GameState::MainMenu);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Quitting);
    
    ImGui::Dummy(ImVec2(0, gap));

    ImGui::PopFont();
    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

static void DrawGameOverMenu()
{
    ImGuiIO& io = ImGui::GetIO();
    AdvanceGameOverAnimation(io.DeltaTime);

    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;

    float title_alpha = 1.0f;
    if (s_game_over_anim.phase == GameOverAnimPhase::TitleReveal)
        title_alpha = EaseInOutCubic(s_game_over_anim.phase_time / GAME_OVER_TITLE_REVEAL_SEC);

    float menu_alpha = 0.0f;
    if (s_game_over_anim.phase == GameOverAnimPhase::FadeInMenuButtons)
        menu_alpha = EaseInOutCubic(s_game_over_anim.phase_time / GAME_OVER_MENU_FADE_SEC);
    else if (s_game_over_anim.phase == GameOverAnimPhase::MenuReady)
        menu_alpha = 1.0f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 180));

    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.28f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, title_alpha);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##gameover_title", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(GameUI_GetFont(GameFont::Title), 120.0f);
    ImGui::TextUnformatted("GAME OVER");
    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const float btn_w = 320.0f;
    const float btn_h = 48.0f;
    const float gap = 16.0f;
    const float pad_v = 2.0f;
    const float total_h = pad_v * 2.0f + btn_h * 2.0f + gap * 2.0f;

    const float bx = sw * 0.5f - btn_w * 0.5f;
    const float by = sh * 0.52f;

    ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btn_w, total_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menu_alpha);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, pad_v));
    ImGui::Begin("##gameover_btns", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ((s_game_over_anim.phase == GameOverAnimPhase::MenuReady) ? 0 : ImGuiWindowFlags_NoInputs));

    PushMenuButtonStyle();
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 30.0f);

    if (ImGui::Button("Restart", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Playing);
    ImGui::SetItemDefaultFocus();

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Return to Main Menu", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::MainMenu);

    ImGui::Dummy(ImVec2(0, gap));

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
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    // Semi-transparent dark overlay over 3D scene
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, 190));

    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.20f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##pause_title", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImFont* title_font = GameUI_GetFont(GameFont::Title);
    if (title_font)
        ImGui::PushFont(title_font, 72.0f);
    ImGui::TextUnformatted("PAUSED");
    if (title_font)
        ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    const float btn_w  = 320.0f;
    const float btn_h  = 48.0f;
    const float gap    = 16.0f;
    const float pad_v  = 2.0f;
    const float total_h = btn_h * 4.0f + gap * 5.0f + pad_v * 2.0f;

    float bx = sw * 0.5f - btn_w * 0.5f;
    float by = sh * 0.42f;

    ImGui::SetNextWindowPos(ImVec2(bx, by), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btn_w, total_h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, pad_v));
    ImGui::Begin("##pausemenu_btns", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    PushMenuButtonStyle();
    ImGui::PushFont(GameUI_GetFont(GameFont::Body), 30.0f);

    if (ImGui::Button("Continue", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Playing);
    ImGui::SetItemDefaultFocus();

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Settings", ImVec2(btn_w, btn_h)))
        OpenOptionsPanel(GameState::Paused);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Return to Main Menu", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::MainMenu);

    ImGui::Dummy(ImVec2(0, gap));

    if (ImGui::Button("Quit Game", ImVec2(btn_w, btn_h)))
        GameUI_SetState(GameState::Quitting);

    ImGui::Dummy(ImVec2(0, gap));

    ImGui::PopFont();

    PopMenuButtonStyle();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ============================================================
//  Public API
// ============================================================

void GameUI_Init()
{
    s_state = GameState::MainMenu;
    ResetMainMenuAnimation();
    ResetGameOverAnimation();
    s_options_panel.active = false;
    s_options_panel.return_state = GameState::MainMenu;
    s_options_panel.selected_section = OptionsPanelSection::None;
    s_skill_choice.active = false;
    s_skill_choice.scene = nullptr;
    s_skill_choice.phase = SkillChoicePhase::LevelIntro;
    s_skill_choice.phase_time = 0.0f;
    s_skill_choice.selected_index = -1;
    ImGuiIO& io = ImGui::GetIO();
    if (io.FontDefault == nullptr)
        io.FontDefault = io.Fonts->AddFontDefault();

    // Load custom fonts here via GameUI_LoadFont() before the first frame.
    // Example:
    GameUI_LoadFont(GameFont::Title, "assets/UI/title fonts/HelpMe.ttf", 100.0f);
    GameUI_LoadFont(GameFont::Body,  "assets/UI/menu fonts/SplendidB.ttf",  32.0f);
    //   GameUI_LoadFont(GameFont::Mono,  "assets/fonts/my_mono.ttf",  18.0f);
    // Unloaded slots fall back to the ImGui built-in default font automatically.

    Renderer_LoadUITexture(HUD_HEART_ICON_PATH, true, &s_playing_hud_resources.heart_icon);
    Renderer_LoadUITexture(HUD_LAYER_BANNER_PATH, true, &s_playing_hud_resources.layer_banner);
    Renderer_LoadUITexture(HUD_DAMAGE_OVERLAY_PATH, false, &s_playing_hud_resources.damage_overlay);

    s_playing_hud.life_count = 0;
    s_playing_hud.loaded_bullets = 0;
    s_playing_hud.backup_bullets = 0;
    s_playing_hud.damage_flash_time = 0.0f;
}

void GameUI_Update()
{
    // While Debug UI is open, freeze game UI state transitions.
    // This keeps game UI hidden and restores its previous state when debug UI closes.
    if (DebugUI_IsOpen())
        return;

    if (s_options_panel.active)
    {
        if (Input_IsActionJustPressed(ACTION_PAUSE))
        {
            CloseOptionsPanel();
        }
        else if (IsOptionsBackJustPressed())
        {
            NavigateOptionsBack();
        }
        return;
    }

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

    if (s_options_panel.active)
    {
        DrawOptionsPanel();
        return;
    }

    switch (s_state)
    {
    case GameState::MainMenu: DrawMainMenu(); break;
    case GameState::Paused:   DrawPauseMenu(); break;
    case GameState::GameOver: DrawGameOverMenu(); break;
    case GameState::Playing: 
        DrawPlayingHUD();
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

    if (s_state != GameState::GameOver && state == GameState::GameOver)
        ResetGameOverAnimation();

    if (state != GameState::MainMenu && state != GameState::Paused)
        CloseOptionsPanel();

    if (state != GameState::Playing)
    {
        s_options_panel.awaiting_binding = false;
        s_options_panel.awaiting_action = ACTION_COUNT;
        s_options_panel.awaiting_device_group = INPUT_BINDING_DEVICE_KEYBOARD_MOUSE;
        s_options_panel.request_nav_focus = false;
        s_playing_hud.damage_flash_time = 0.0f;
        s_skill_choice.active = false;
        s_skill_choice.phase = SkillChoicePhase::LevelIntro;
        s_skill_choice.phase_time = 0.0f;
        s_skill_choice.selected_index = -1;
    }

    s_state = state;
}
