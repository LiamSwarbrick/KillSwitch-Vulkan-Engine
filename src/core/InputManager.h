// InputManager.h
#pragma once
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>
#include <array>

class InputManager {
public:
    static InputManager& GetInstance();

    void Update();                    
    void ProcessEvent(const SDL_Event& e);  
    void UpdateGamepadState();        

    bool IsKeyDown(SDL_Scancode key) const;
    bool IsKeyPressed(SDL_Scancode key) const;   
    bool IsKeyReleased(SDL_Scancode key) const;

    bool IsMouseButtonDown(Uint8 button) const;
    bool IsMouseButtonPressed(Uint8 button) const;   
    bool IsMouseButtonReleased(Uint8 button) const;
    void GetMouseDelta(int& dx, int& dy) const;
    void GetMousePosition(int& x, int& y) const;

    void AddAction(const std::string& actionName, SDL_Scancode key, Uint8 mouseBtn = 0);
    bool IsActionPressed(const std::string& actionName) const;
    bool IsActionHeld(const std::string& actionName) const;

    bool IsGamepadConnected() const;
    bool IsGamepadButtonDown(SDL_GamepadButton btn) const;
    bool IsGamepadButtonPressed(SDL_GamepadButton btn) const; 
    bool IsGamepadButtonReleased(SDL_GamepadButton btn) const;
    float GetGamepadAxis(SDL_GamepadAxis axis) const;

    void PrintKeyboardEvent(const SDL_KeyboardEvent& key);
    void PrintMouseMotionEvent(const SDL_MouseMotionEvent& motion);
    void PrintMouseButtonEvent(const SDL_MouseButtonEvent& button);
    void PrintMouseWheelEvent(const SDL_MouseWheelEvent& wheel);
    void PrintGamepadButtonEvent(const SDL_GamepadButtonEvent& button);
    void PrintGamepadAxisEvent(const SDL_GamepadAxisEvent& axis);

private:
    InputManager();
    ~InputManager();

    const Uint8* m_currentKeyboardState = nullptr;
    std::array<Uint8, SDL_SCANCODE_COUNT> m_prevKeyboardState{};

    Uint32 m_currentMouseState = 0;
    Uint32 m_prevMouseState = 0;
    int m_mouseX = 0, m_mouseY = 0;
    int m_mouseDeltaX = 0, m_mouseDeltaY = 0;

    struct ActionMapping {
        SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
        Uint8 mouseButton = 0;
    };
    std::unordered_map<std::string, ActionMapping> m_actionMap;

    SDL_JoystickID m_gamepadId = -1;
    std::unordered_map<int, bool> m_prevGamepadButtonState;
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> m_lastGamepadAxisValues{};

    SDL_Gamepad* m_gamepad = nullptr;

    
    static const char* SafeString(const char* value, const char* fallback);
    static const char* MouseButtonName(Uint8 button);
    static float NormalizeGamepadAxis(Uint8 axis, Sint16 value);
    static bool ShouldPrintAxisChange(Sint16 previous, Sint16 current);
};