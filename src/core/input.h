#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include "SDL3/SDL.h"
#include <stdint.h>




// Action enums for button mapping
enum InputAction : uint32_t
{
    // Movement
    ACTION_MOVE_FORWARD,
    ACTION_MOVE_BACKWARD,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_SPRINT,
    ACTION_JUMP,

    // Camera
    ACTION_CAMERA_UP,
    ACTION_CAMERA_DOWN,
    ACTION_CAMERA_LEFT,
    ACTION_CAMERA_RIGHT,

    // Gameplay
    ACTION_INTERACT,
    ACTION_ATTACK,

    // UI / System
    ACTION_PAUSE,
    ACTION_DEBUG_TOGGLE,

    ACTION_COUNT  // must be last
};

// Binding types
enum BindingSource : uint8_t
{
    BIND_KEYBOARD,
    BIND_MOUSE_BUTTON,
    BIND_GAMEPAD_BUTTON,
    BIND_GAMEPAD_AXIS_POS,   // stick right&up/trigger
    BIND_GAMEPAD_AXIS_NEG    // stick left&down/trigger
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
float Input_GetActionValue(InputAction action);       // 0.0–1.0 (analog or digital)

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

    // Utility
const char* Input_GetActionName(InputAction action);
const char* Input_GetBindingDisplayName(const InputBinding& binding);

#endif // CORE_INPUT_H
