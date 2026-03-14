#include "core/core.h"
#include "renderer/renderer.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_main.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace
{
struct GamepadDebugState
{
    SDL_Gamepad* handle = nullptr;
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> last_axis_values{};
};

using GamepadMap = std::unordered_map<SDL_JoystickID, GamepadDebugState>;

const char* SafeString(const char* value, const char* fallback)
{
    return (value && value[0] != '\0') ? value : fallback;
}

const char* MouseButtonName(Uint8 button)
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

float NormalizeGamepadAxis(Uint8 axis, Sint16 value)
{
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
    {
        return static_cast<float>(value) / 32767.0f;
    }

    return static_cast<float>(value) / 32768.0f;
}

bool ShouldPrintAxisChange(Sint16 previous, Sint16 current)
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

void PrintKeyboardEvent(const SDL_KeyboardEvent& key)
{
    std::printf(
        "[Keyboard] %s scancode=%s key=%s repeat=%s\n",
        key.down ? "down" : "up",
        SafeString(SDL_GetScancodeName(key.scancode), "Unknown"),
        SafeString(SDL_GetKeyName(key.key), "Unknown"),
        key.repeat ? "yes" : "no"
    );
}

void PrintMouseMotionEvent(const SDL_MouseMotionEvent& motion)
{
    std::printf(
        "[Mouse] motion x=%.1f y=%.1f dx=%.1f dy=%.1f\n",
        motion.x,
        motion.y,
        motion.xrel,
        motion.yrel
    );
}

void PrintMouseButtonEvent(const SDL_MouseButtonEvent& button)
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

void PrintMouseWheelEvent(const SDL_MouseWheelEvent& wheel)
{
    std::printf(
        "[Mouse] wheel x=%.2f y=%.2f ticks=(%d,%d)\n",
        wheel.x,
        wheel.y,
        wheel.integer_x,
        wheel.integer_y
    );
}

void OpenGamepad(GamepadMap& gamepads, SDL_JoystickID instance_id)
{
    if (gamepads.find(instance_id) != gamepads.end())
    {
        return;
    }

    SDL_Gamepad* handle = SDL_OpenGamepad(instance_id);
    if (!handle)
    {
        std::printf(
            "[Gamepad %d] failed to open: %s\n",
            instance_id,
            SafeString(SDL_GetError(), "Unknown error")
        );
        return;
    }

    const char* name = SDL_GetGamepadName(handle);
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadType(handle));

    gamepads[instance_id].handle = handle;

    std::printf(
        "[Gamepad %d] connected name=%s type=%s\n",
        instance_id,
        SafeString(name, "Unknown"),
        SafeString(type, "unknown")
    );
}

void CloseGamepad(GamepadMap& gamepads, SDL_JoystickID instance_id)
{
    auto it = gamepads.find(instance_id);
    if (it == gamepads.end())
    {
        std::printf("[Gamepad %d] disconnected\n", instance_id);
        return;
    }

    if (it->second.handle)
    {
        SDL_CloseGamepad(it->second.handle);
    }

    gamepads.erase(it);
    std::printf("[Gamepad %d] disconnected\n", instance_id);
}

void OpenAlreadyConnectedGamepads(GamepadMap& gamepads)
{
    int count = 0;
    SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&count);
    if (!gamepad_ids)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        OpenGamepad(gamepads, gamepad_ids[i]);
    }

    SDL_free(gamepad_ids);
}

void PrintGamepadButtonEvent(const SDL_GamepadButtonEvent& button)
{
    std::printf(
        "[Gamepad %d] %s button=%s\n",
        button.which,
        button.down ? "down" : "up",
        SafeString(SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(button.button)), "unknown-button")
    );
}

void PrintGamepadAxisEvent(const SDL_GamepadAxisEvent& axis_event, GamepadMap& gamepads)
{
    auto& state = gamepads[axis_event.which];
    const Sint16 previous = state.last_axis_values[axis_event.axis];
    state.last_axis_values[axis_event.axis] = axis_event.value;

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

void CloseAllGamepads(GamepadMap& gamepads)
{
    for (auto& [instance_id, state] : gamepads)
    {
        if (state.handle)
        {
            SDL_CloseGamepad(state.handle);
        }
        std::printf("[Gamepad %d] disconnected\n", instance_id);
    }

    gamepads.clear();
}
}  // namespace

int main(int argc, char *argv[])
{
    bool is_debugging = true;
#ifdef NDEBUG
    is_debugging = false;
#endif

    SDL_Window* window = Core_Init((Core_InitInfo){
        "Close-quarters Adventure Game",
        1280, 720
    });

    Renderer_InitInfo renderer_info = { .window = window, .enable_validation = is_debugging };
    Renderer_Init(&renderer_info);

    GamepadMap gamepads;
    OpenAlreadyConnectedGamepads(gamepads);
    
    // Testing shader upload api.
    // const uint32_t* test_vert_spv = {};    
    // uint16_t shader_id = Renderer_RegisterShaders()

    bool running = true;
    while (running)
    {
        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            switch (event.type)
            {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    PrintKeyboardEvent(event.key);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    PrintMouseMotionEvent(event.motion);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    PrintMouseButtonEvent(event.button);
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    PrintMouseWheelEvent(event.wheel);
                    break;

                case SDL_EVENT_GAMEPAD_ADDED:
                    OpenGamepad(gamepads, event.gdevice.which);
                    break;

                case SDL_EVENT_GAMEPAD_REMOVED:
                    CloseGamepad(gamepads, event.gdevice.which);
                    break;

                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    PrintGamepadButtonEvent(event.gbutton);
                    break;

                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    PrintGamepadAxisEvent(event.gaxis, gamepads);
                    break;

                default:
                    break;
            }

            Renderer_ListenToWindowEvent(event);
        }

        // Game ticks
        // TODO:

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            // NOTE: Passes will gather their own renderables, reason being, some passes will just draw a hardcoded full screen triangle,
            // others will draw from the lights perspective, and others will only draw the toon shaded characters for example
            //
            // TODO: Gather entity renderables in RenderView, put this as a function in renderer/renderpasses/gather_renderables.cpp or something
            // Later TODO: Gather visible entities only for extra optimization.
            // with support for different passes e.g. shadows from lights perspective will require different entity lists.
            // In future, when entity system sorted out, can move entity gathering
            // to inside the Renderer_DrawFrame function, and this can use core
            // systems to gather relevant entities per each render pass.
            
            Renderer_DrawFrame();
        }
    }

    CloseAllGamepads(gamepads);
    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
