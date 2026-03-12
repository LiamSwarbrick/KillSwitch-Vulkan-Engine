#include "../InputManager.h"
#include <iostream>
#include <cstring>
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
