#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include "SDL3/SDL.h"
#include "core/input_actions.h"

// Binding types
enum BindingSource : uint8_t
{
    BIND_KEYBOARD,
    BIND_MOUSE_BUTTON,
    BIND_GAMEPAD_BUTTON,
    BIND_GAMEPAD_AXIS_POS,   // stick right&up/trigger
    BIND_GAMEPAD_AXIS_NEG    // stick left&down/trigger
};

enum InputBindingDeviceGroup : uint8_t
{
    INPUT_BINDING_DEVICE_KEYBOARD_MOUSE,
    INPUT_BINDING_DEVICE_GAMEPAD,
};

// Binding values
struct InputBinding
{
    BindingSource source;
    union {
        SDL_Scancode    scancode;       // BIND_KEYBOARD
        uint8_t         mouse_button;   // BIND_MOUSE_BUTTON
        SDL_GamepadButton pad_button;   // BIND_GAMEPAD_BUTTON
        SDL_GamepadAxis   pad_axis;     // BIND_GAMEPAD_AXIS_POS / NEG
    };
};

// Max bindings per action (keyboard + gamepad fallback)
#define INPUT_MAX_BINDINGS_PER_ACTION 4

// Public API
    // Init/Shutdown
void  Input_Init(const char* bindings_path);    // if NULL use defaults only
void  Input_Shutdown();

    // Per-frame event processing and state update
void  Input_ProcessEvent(const SDL_Event& event);   // Call inside SDL_PollEvent loop
void  Input_Update();                                // Call once after all events processed

    // Queries (valid after Input_Update)
bool  Input_IsActionPressed(InputAction action);      // Held this frame
bool  Input_IsActionJustPressed(InputAction action);  // Pressed this frame, not last
bool  Input_IsActionJustReleased(InputAction action); // Released this frame
bool  Input_WasAnyInputJustPressed();                 // Any keyboard/mouse/gamepad button pressed this frame
float Input_GetActionValue(InputAction action);       // 0.0–1.0 (analog or digital)
bool  Input_IsKeyJustPressed(SDL_Scancode scancode);
bool  Input_IsGamepadButtonJustPressed(SDL_GamepadButton button);

    // Mouse delta 
void  Input_GetMouseDelta(float* dx, float* dy);

    // Gamepad connection
bool  Input_IsGamepadConnected();

    // Binding management
void  Input_SetBinding(InputAction action, int slot, InputBinding binding);
InputBinding Input_GetBinding(InputAction action, int slot);
int   Input_GetBindingCount(InputAction action);
void  Input_ClearBindings(InputAction action);

    // Save/load bindings to JSON file
bool  Input_SaveBindings(const char* path);
bool  Input_LoadBindings(const char* path);

    // Rebind capture helpers
bool  Input_PollPendingBinding(InputBindingDeviceGroup device_group, InputBinding* out_binding);
void  Input_ClearPendingBindingCapture();

    // Utility
const char* Input_GetActionName(InputAction action);
const char* Input_GetBindingDisplayName(const InputBinding& binding);

#endif // CORE_INPUT_H
