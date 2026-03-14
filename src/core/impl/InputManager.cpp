#include "../InputManager.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <SDL3/SDL_gamepad.h>

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

InputManager::InputManager()
    : m_gamepadId(-1)
{
    int count = 0;
    m_currentKeyboardState = reinterpret_cast<const Uint8*>(SDL_GetKeyboardState(&count));
    std::memset(m_prevKeyboardState.data(), 0, m_prevKeyboardState.size());
    
    SDL_Init(SDL_INIT_GAMEPAD);
    
    int gamepad_count = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&gamepad_count);
    if (joysticks && gamepad_count > 0) {
        m_gamepadId = joysticks[0];
    }
    SDL_free(joysticks);
}

InputManager::~InputManager()
{
    if (m_gamepadId >= 0) {
        SDL_CloseGamepad(SDL_GetGamepadFromID(m_gamepadId));
    }
    SDL_Quit();
}

void InputManager::Update()
{
    int count = 0;
    const Uint8* current = reinterpret_cast<const Uint8*>(SDL_GetKeyboardState(&count));
    if (current && count > 0) {
        std::memcpy(m_prevKeyboardState.data(), current, 
                   std::min(static_cast<size_t>(count), m_prevKeyboardState.size()));
    }
    m_currentKeyboardState = current;
    
    m_prevMouseState = m_currentMouseState;
    float mouseX = 0.0f, mouseY = 0.0f;
    m_currentMouseState = SDL_GetMouseState(&mouseX, &mouseY);
    m_mouseX = static_cast<int>(mouseX);
    m_mouseY = static_cast<int>(mouseY);
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
}

void InputManager::ProcessEvent(const SDL_Event& e)
{
    switch (e.type) {
        case SDL_EVENT_MOUSE_MOTION:
            m_mouseDeltaX = e.motion.xrel;
            m_mouseDeltaY = e.motion.yrel;
            break;
            
        case SDL_EVENT_GAMEPAD_ADDED:
            if (m_gamepadId < 0) {
                m_gamepadId = e.gdevice.which;
                std::cout << "Game Pad Connected: ID " << m_gamepadId << std::endl;
            }
            break;
            
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (m_gamepadId == e.gdevice.which) {
                m_gamepadId = -1;
                std::cout << "Game Pad Disconnected" << std::endl;
            }
            break;
            
        default:
            break;
    }
}

bool InputManager::IsKeyDown(SDL_Scancode key) const
{
    if (!m_currentKeyboardState) return false;
    return m_currentKeyboardState[key] != 0;
}

bool InputManager::IsKeyPressed(SDL_Scancode key) const
{
    if (!m_currentKeyboardState) return false;
    return m_currentKeyboardState[key] != 0 && m_prevKeyboardState[key] == 0;
}

bool InputManager::IsKeyReleased(SDL_Scancode key) const
{
    if (!m_currentKeyboardState) return false;
    return m_currentKeyboardState[key] == 0 && m_prevKeyboardState[key] != 0;
}

bool InputManager::IsMouseButtonDown(Uint8 button) const
{
    return (m_currentMouseState & SDL_BUTTON_MASK(button)) != 0;
}

bool InputManager::IsMouseButtonPressed(Uint8 button) const
{
    return ((m_currentMouseState & SDL_BUTTON_MASK(button)) != 0) &&
           ((m_prevMouseState & SDL_BUTTON_MASK(button)) == 0);
}

bool InputManager::IsMouseButtonReleased(Uint8 button) const
{
    return ((m_currentMouseState & SDL_BUTTON_MASK(button)) == 0) &&
           ((m_prevMouseState & SDL_BUTTON_MASK(button)) != 0);
}

void InputManager::GetMouseDelta(int& dx, int& dy) const
{
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}

void InputManager::GetMousePosition(int& x, int& y) const
{
    x = m_mouseX;
    y = m_mouseY;
}

void InputManager::AddAction(const std::string& actionName, SDL_Scancode key, Uint8 mouseBtn)
{
    m_actionMap[actionName] = ActionMapping{ key, mouseBtn };
}

bool InputManager::IsActionPressed(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it == m_actionMap.end()) return false;
    
    const auto& mapping = it->second;
    if (mapping.key != SDL_SCANCODE_UNKNOWN && IsKeyPressed(mapping.key)) {
        return true;
    }
    if (mapping.mouseButton != 0 && IsMouseButtonPressed(mapping.mouseButton)) {
        return true;
    }
    
    return false;
}

bool InputManager::IsActionHeld(const std::string& actionName) const
{
    auto it = m_actionMap.find(actionName);
    if (it == m_actionMap.end()) return false;
    
    const auto& mapping = it->second;
    if (mapping.key != SDL_SCANCODE_UNKNOWN && IsKeyDown(mapping.key)) {
        return true;
    }
    if (mapping.mouseButton != 0 && IsMouseButtonDown(mapping.mouseButton)) {
        return true;
    }
    
    return false;
}

bool InputManager::IsGamepadConnected() const
{
    return m_gamepadId >= 0;
}

bool InputManager::IsGamepadButtonPressed(SDL_GamepadButton btn) const
{
    if (m_gamepadId < 0) return false;
    
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_gamepadId);
    if (!gamepad) return false;
    
    
    return SDL_GetGamepadButton(gamepad, btn) != 0;
}

bool InputManager::IsGamepadButtonDown(SDL_GamepadButton btn) const
{
    if (m_gamepadId < 0) return false;
    
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_gamepadId);
    if (!gamepad) return false;
    
    return SDL_GetGamepadButton(gamepad, btn) != 0;
}

bool InputManager::IsGamepadButtonReleased(SDL_GamepadButton btn) const
{
    if (m_gamepadId < 0) return false;
    
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_gamepadId);
    if (!gamepad) return false;
    
  
    auto it = m_prevGamepadButtonState.find(btn);
    bool wasPressedLastFrame = (it != m_prevGamepadButtonState.end()) ? it->second : false;
    bool isPressedNow = SDL_GetGamepadButton(gamepad, btn) != 0;
    
    return !isPressedNow && wasPressedLastFrame;
}

float InputManager::GetGamepadAxis(SDL_GamepadAxis axis) const
{
    if (m_gamepadId < 0) return 0.0f;
    
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_gamepadId);
    if (!gamepad) return 0.0f;
    
    
    int16_t value = SDL_GetGamepadAxis(gamepad, axis);
    return static_cast<float>(value) / 32767.0f;
}

void InputManager::UpdateGamepadState()
{
    if (m_gamepadId < 0) return;
    
    m_prevGamepadButtonState.clear();
    
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_gamepadId);
    if (!gamepad) return;
    
    SDL_GamepadButton buttons[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };
    
    for (auto btn : buttons) {
        m_prevGamepadButtonState[btn] = SDL_GetGamepadButton(gamepad, btn) != 0;
    }
}

// === 调试函数实现 ===

const char* InputManager::SafeString(const char* value, const char* fallback)
{
    return (value && value[0] != '\0') ? value : fallback;
}

const char* InputManager::MouseButtonName(Uint8 button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:   return "Left";
        case SDL_BUTTON_MIDDLE: return "Middle";
        case SDL_BUTTON_RIGHT:  return "Right";
        case SDL_BUTTON_X1:     return "X1";
        case SDL_BUTTON_X2:     return "X2";
        default:                return "Unknown";
    }
}

float InputManager::NormalizeGamepadAxis(Uint8 axis, Sint16 value)
{
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
    {
        return static_cast<float>(value) / 32767.0f;
    }
    return static_cast<float>(value) / 32768.0f;
}

bool InputManager::ShouldPrintAxisChange(Sint16 previous, Sint16 current)
{
    constexpr int kDeadzone = 8000;
    constexpr int kDeltaThreshold = 4000;

    const bool previous_active = std::abs(previous) >= kDeadzone;
    const bool current_active = std::abs(current) >= kDeadzone;
    if (previous_active != current_active)
    {
        return true;
    }

    return current_active && std::abs(current - previous) >= kDeltaThreshold;
}

void InputManager::PrintKeyboardEvent(const SDL_KeyboardEvent& key)
{
    std::printf(
        "[Keyboard] %s scancode=%s key=%s repeat=%s\n",
        key.down ? "down" : "up",
        SafeString(SDL_GetScancodeName(key.scancode), "Unknown"),
        SafeString(SDL_GetKeyName(key.key), "Unknown"),
        key.repeat ? "yes" : "no"
    );
}

void InputManager::PrintMouseMotionEvent(const SDL_MouseMotionEvent& motion)
{
    std::printf(
        "[Mouse] motion x=%.1f y=%.1f dx=%.1f dy=%.1f\n",
        motion.x,
        motion.y,
        motion.xrel,
        motion.yrel
    );
}

void InputManager::PrintMouseButtonEvent(const SDL_MouseButtonEvent& button)
{
    std::printf(
        "[Mouse] %s button=%s clicks=%u x=%.1f y=%.1f\n",
        button.down ? "down" : "up",
        MouseButtonName(button.button),
        static_cast<unsigned>(button.clicks),
        button.x,
        button.y
    );
}

void InputManager::PrintMouseWheelEvent(const SDL_MouseWheelEvent& wheel)
{
    std::printf(
        "[Mouse] wheel x=%.2f y=%.2f ticks=(%d,%d)\n",
        wheel.x,
        wheel.y,
        wheel.integer_x,
        wheel.integer_y
    );
}

void InputManager::PrintGamepadButtonEvent(const SDL_GamepadButtonEvent& button)
{
    std::printf(
        "[Gamepad %d] %s button=%s\n",
        button.which,
        button.down ? "down" : "up",
        SafeString(SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(button.button)), "unknown-button")
    );
}

void InputManager::PrintGamepadAxisEvent(const SDL_GamepadAxisEvent& axis_event)
{
    if (m_gamepadId != axis_event.which)
    {
        m_gamepadId = axis_event.which;
        std::fill(m_lastGamepadAxisValues.begin(), m_lastGamepadAxisValues.end(), 0);
    }

    const Sint16 previous = m_lastGamepadAxisValues[axis_event.axis];
    m_lastGamepadAxisValues[axis_event.axis] = axis_event.value;

    if (!ShouldPrintAxisChange(previous, axis_event.value))
    {
        return;
    }

    std::printf(
        "[Gamepad %d] axis=%s value=%d normalized=%.3f\n",
        axis_event.which,
        SafeString(SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(axis_event.axis)), "unknown-axis"),
        static_cast<int>(axis_event.value),
        NormalizeGamepadAxis(axis_event.axis, axis_event.value)
    );
}
