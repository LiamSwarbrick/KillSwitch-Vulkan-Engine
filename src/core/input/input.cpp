#include "core/input.h"
#include "SDL3/SDL.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <cstring>
#include <cstdio>
#include <cmath>

namespace rj = rapidjson;

//  Internal state
struct ActionState // per frame
{
    float    value;          // 0.0 – 1.0 (analog-aware)
    bool     pressed;        // held this frame
    bool     prev_pressed;   // held last frame
};

struct ActionBindings // per action
{
    InputBinding bindings[INPUT_MAX_BINDINGS_PER_ACTION];
    int          count;
};

static ActionBindings  s_bindings[ACTION_COUNT]  = {};
static ActionState     s_actions[ACTION_COUNT]   = {};

// Keyboard bool values from SDL_GetKeyboardState
static const bool*     s_keyboard_state = nullptr;

// Mouse related values
static float           s_mouse_dx = 0.0f;
static float           s_mouse_dy = 0.0f;
static float           s_mouse_accum_x = 0.0f;
static float           s_mouse_accum_y = 0.0f;
static uint32_t        s_mouse_buttons = 0;

// Gamepad connection state
static SDL_Gamepad*    s_gamepad = nullptr;
static SDL_JoystickID  s_gamepad_id = 0;

// Axis default deadzone, for now just keeped hardcoded here, can be made configurable if needed
static constexpr float AXIS_DEADZONE = 0.20f;

// Action name table — generated directly from INPUT_ACTIONS_LIST in input_actions.h.
// Always in sync with the enum; no manual maintenance needed.
static const char* s_action_names[] = {
#define X(suffix, name) name,
    INPUT_ACTIONS_LIST
#undef X
};

//  Default bindings
static void add_binding(InputAction action, BindingSource source, int code)
{
    ActionBindings& ab = s_bindings[action];
    if (ab.count >= INPUT_MAX_BINDINGS_PER_ACTION) return;
    InputBinding b = {};
    b.source = source;
    switch (source)
    {
        case BIND_KEYBOARD:         b.scancode   = (SDL_Scancode)code;      break;
        case BIND_MOUSE_BUTTON:     b.mouse_button = (uint8_t)code;         break;
        case BIND_GAMEPAD_BUTTON:   b.pad_button = (SDL_GamepadButton)code; break;
        case BIND_GAMEPAD_AXIS_POS: b.pad_axis   = (SDL_GamepadAxis)code;   break;
        case BIND_GAMEPAD_AXIS_NEG: b.pad_axis   = (SDL_GamepadAxis)code;   break;
    }
    ab.bindings[ab.count++] = b;
}

static void set_default_bindings()
{
    for (int i = 0; i < ACTION_COUNT; i++)
        s_bindings[i].count = 0;

    // Movement – keyboard
    add_binding(ACTION_MOVE_FORWARD,  BIND_KEYBOARD, SDL_SCANCODE_W);
    add_binding(ACTION_MOVE_BACKWARD, BIND_KEYBOARD, SDL_SCANCODE_S);
    add_binding(ACTION_MOVE_LEFT,     BIND_KEYBOARD, SDL_SCANCODE_A);
    add_binding(ACTION_MOVE_RIGHT,    BIND_KEYBOARD, SDL_SCANCODE_D);
    add_binding(ACTION_MOVE_UP,       BIND_KEYBOARD, SDL_SCANCODE_E);
    add_binding(ACTION_MOVE_DOWN,     BIND_KEYBOARD, SDL_SCANCODE_Q);
    add_binding(ACTION_SPRINT,        BIND_KEYBOARD, SDL_SCANCODE_LSHIFT);
    add_binding(ACTION_JUMP,          BIND_KEYBOARD, SDL_SCANCODE_SPACE);

    // Movement – gamepad
    add_binding(ACTION_MOVE_FORWARD,  BIND_GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTY);
    add_binding(ACTION_MOVE_BACKWARD, BIND_GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTY);
    add_binding(ACTION_MOVE_LEFT,     BIND_GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTX);
    add_binding(ACTION_MOVE_RIGHT,    BIND_GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTX);
    add_binding(ACTION_SPRINT,        BIND_GAMEPAD_BUTTON,   SDL_GAMEPAD_BUTTON_LEFT_STICK);
    add_binding(ACTION_JUMP,          BIND_GAMEPAD_BUTTON,   SDL_GAMEPAD_BUTTON_SOUTH);

    // Camera – keyboard
    add_binding(ACTION_CAMERA_UP,    BIND_KEYBOARD, SDL_SCANCODE_UP);
    add_binding(ACTION_CAMERA_DOWN,  BIND_KEYBOARD, SDL_SCANCODE_DOWN);
    add_binding(ACTION_CAMERA_LEFT,  BIND_KEYBOARD, SDL_SCANCODE_LEFT);
    add_binding(ACTION_CAMERA_RIGHT, BIND_KEYBOARD, SDL_SCANCODE_RIGHT);

    // Camera – gamepad right stick
    add_binding(ACTION_CAMERA_UP,    BIND_GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTY);
    add_binding(ACTION_CAMERA_DOWN,  BIND_GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTY);
    add_binding(ACTION_CAMERA_LEFT,  BIND_GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTX);
    add_binding(ACTION_CAMERA_RIGHT, BIND_GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTX);

    // Gameplay
    add_binding(ACTION_INTERACT, BIND_KEYBOARD,       SDL_SCANCODE_F);
    add_binding(ACTION_INTERACT, BIND_GAMEPAD_BUTTON,  SDL_GAMEPAD_BUTTON_WEST);
    add_binding(ACTION_ATTACK,   BIND_MOUSE_BUTTON,    SDL_BUTTON_LEFT);
    add_binding(ACTION_ATTACK,   BIND_GAMEPAD_BUTTON,  SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    // UI
    add_binding(ACTION_PAUSE,        BIND_KEYBOARD,       SDL_SCANCODE_ESCAPE);
    add_binding(ACTION_PAUSE,        BIND_GAMEPAD_BUTTON,  SDL_GAMEPAD_BUTTON_START);
    add_binding(ACTION_DEBUG_TOGGLE, BIND_KEYBOARD,       SDL_SCANCODE_F3);
}

//  Gamepad connection handling

static void try_open_gamepad()
{
    if (s_gamepad) return;
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&count);
    if (joysticks && count > 0)
    {
        s_gamepad = SDL_OpenGamepad(joysticks[0]);
        if (s_gamepad)
        {
            s_gamepad_id = joysticks[0];
            SDL_Log("Input: Gamepad connected:  %s", SDL_GetGamepadName(s_gamepad));
        }
    }
    SDL_free(joysticks);
}

static void close_gamepad()
{
    if (s_gamepad)
    {
        SDL_CloseGamepad(s_gamepad);
        s_gamepad = nullptr;
        s_gamepad_id = 0;
        SDL_Log("Input: Gamepad disconnected");
    }
}

//  Binding evaluation
//      Returns a value from 0.0 to 1.0 for analog inputs, 0.0/1.0 for digital inputs.
static float evaluate_binding(const InputBinding& b)
{
    switch (b.source)
    {
    case BIND_KEYBOARD:
        if (s_keyboard_state && s_keyboard_state[b.scancode])
            return 1.0f;
        return 0.0f;

    case BIND_MOUSE_BUTTON:
        if (s_mouse_buttons & SDL_BUTTON_MASK(b.mouse_button))
            return 1.0f;
        return 0.0f;

    case BIND_GAMEPAD_BUTTON:
        if (s_gamepad && SDL_GetGamepadButton(s_gamepad, b.pad_button))
            return 1.0f;
        return 0.0f;

    case BIND_GAMEPAD_AXIS_POS:
    {
        if (!s_gamepad) return 0.0f;
        float raw = SDL_GetGamepadAxis(s_gamepad, b.pad_axis) / 32767.0f;
        if (raw < AXIS_DEADZONE) return 0.0f;
        return (raw - AXIS_DEADZONE) / (1.0f - AXIS_DEADZONE);
    }

    case BIND_GAMEPAD_AXIS_NEG:
    {
        if (!s_gamepad) return 0.0f;
        float raw = SDL_GetGamepadAxis(s_gamepad, b.pad_axis) / 32767.0f;
        if (raw > -AXIS_DEADZONE) return 0.0f;
        return (-raw - AXIS_DEADZONE) / (1.0f - AXIS_DEADZONE);
    }
    }
    return 0.0f;
}

//  JSON stuff for saving/loading bindings

static const char* binding_source_str(BindingSource s) // Convert binding source enum to string for JSON 
{
    switch (s) {
        case BIND_KEYBOARD:         return "keyboard";
        case BIND_MOUSE_BUTTON:     return "mouse_button";
        case BIND_GAMEPAD_BUTTON:   return "gamepad_button";
        case BIND_GAMEPAD_AXIS_POS: return "gamepad_axis_pos";
        case BIND_GAMEPAD_AXIS_NEG: return "gamepad_axis_neg";
    }
    return "unknown";
}

static BindingSource binding_source_from_str(const char* s) // Convert string from JSON back to BindingSource enum
{
    if (strcmp(s, "keyboard")         == 0) return BIND_KEYBOARD;
    if (strcmp(s, "mouse_button")     == 0) return BIND_MOUSE_BUTTON;
    if (strcmp(s, "gamepad_button")   == 0) return BIND_GAMEPAD_BUTTON;
    if (strcmp(s, "gamepad_axis_pos") == 0) return BIND_GAMEPAD_AXIS_POS;
    if (strcmp(s, "gamepad_axis_neg") == 0) return BIND_GAMEPAD_AXIS_NEG;
    return BIND_KEYBOARD;
}

bool Input_SaveBindings(const char* path) // Save current bindings to a JSON file, returns true on success
{
    rj::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    for (int a = 0; a < ACTION_COUNT; a++)
    {
        rj::Value arr(rj::kArrayType);
        for (int i = 0; i < s_bindings[a].count; i++)
        {
            const InputBinding& b = s_bindings[a].bindings[i];
            rj::Value obj(rj::kObjectType);
            obj.AddMember("source", rj::Value(binding_source_str(b.source), alloc), alloc);

            int code = 0;
            switch (b.source) {
                case BIND_KEYBOARD:         code = (int)b.scancode;     break;
                case BIND_MOUSE_BUTTON:     code = (int)b.mouse_button; break;
                case BIND_GAMEPAD_BUTTON:   code = (int)b.pad_button;   break;
                case BIND_GAMEPAD_AXIS_POS: code = (int)b.pad_axis;     break;
                case BIND_GAMEPAD_AXIS_NEG: code = (int)b.pad_axis;     break;
            }
            obj.AddMember("code", code, alloc);

            // readable hint (not used for loading, just for readability)
            const char* name = SDL_GetScancodeName(b.scancode);
            if (b.source == BIND_KEYBOARD && name && name[0])
                obj.AddMember("_hint", rj::Value(name, alloc), alloc);

            arr.PushBack(obj, alloc);
        }
        doc.AddMember(rj::Value(s_action_names[a], alloc), arr, alloc);
    }

    rj::StringBuffer sb;
    rj::PrettyWriter<rj::StringBuffer> writer(sb);
    doc.Accept(writer);

    FILE* f = fopen(path, "wb");
    if (!f)
    {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Input: Failed to save bindings to %s", path);
        return false;
    }
    fwrite(sb.GetString(), 1, sb.GetSize(), f);
    fclose(f);

    SDL_Log("Input: Saved bindings to %s", path);
    return true;
}

bool Input_LoadBindings(const char* path) // Load bindings from a JSON file, returns true on success
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) // sanity limit 1MB
    {
        fclose(f);
        return false;
    }

    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    rj::Document doc;
    doc.Parse(buf);
    free(buf);

    if (doc.HasParseError() || !doc.IsObject())
    {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Input: Failed to parse bindings file %s", path);
        return false;
    }

    for (int a = 0; a < ACTION_COUNT; a++)
    {
        const char* name = s_action_names[a];
        if (!doc.HasMember(name) || !doc[name].IsArray()) continue;

        s_bindings[a].count = 0;
        const rj::Value& arr = doc[name];
        for (rj::SizeType i = 0; i < arr.Size() && s_bindings[a].count < INPUT_MAX_BINDINGS_PER_ACTION; i++)
        {
            const rj::Value& obj = arr[i];
            if (!obj.HasMember("source") || !obj.HasMember("code")) continue;

            InputBinding b = {};
            b.source = binding_source_from_str(obj["source"].GetString());
            int code = obj["code"].GetInt();

            switch (b.source) {
                case BIND_KEYBOARD:         b.scancode     = (SDL_Scancode)code;      break;
                case BIND_MOUSE_BUTTON:     b.mouse_button = (uint8_t)code;           break;
                case BIND_GAMEPAD_BUTTON:   b.pad_button   = (SDL_GamepadButton)code; break;
                case BIND_GAMEPAD_AXIS_POS: b.pad_axis     = (SDL_GamepadAxis)code;   break;
                case BIND_GAMEPAD_AXIS_NEG: b.pad_axis     = (SDL_GamepadAxis)code;   break;
            }
            s_bindings[a].bindings[s_bindings[a].count++] = b;
        }
    }

    SDL_Log("Input: Loaded bindings from %s", path);
    return true;
}

//  Init and shutdown
void Input_Init(const char* bindings_path) 
{
    memset(s_actions,  0, sizeof(s_actions));   // Clear all action states
    memset(s_bindings, 0, sizeof(s_bindings));  // Clear all bindings
    s_mouse_dx = s_mouse_dy = 0.0f;             // Clear mouse state
    s_mouse_accum_x = s_mouse_accum_y = 0.0f;   // Clear mouse accumulators
    s_mouse_buttons = 0;                        // Clear mouse buttons

    set_default_bindings();

    if (bindings_path)
    {
        if (!Input_LoadBindings(bindings_path))
            SDL_Log("Input: No bindings file found at %s, using defaults", bindings_path);
    }

    try_open_gamepad();

    SDL_Log("Input: Initialized (%d actions)", ACTION_COUNT);
}

void Input_Shutdown()
{
    close_gamepad();
    SDL_Log("Input: Shutdown");
}

// Per-frame event processing and state update

void Input_ProcessEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        s_mouse_accum_x += event.motion.xrel;
        s_mouse_accum_y += event.motion.yrel;
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
        try_open_gamepad();
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (event.gdevice.which == s_gamepad_id) // Only close if the removed gamepad is the one we have open
            close_gamepad();
        break;

    default:
        break;
    }
}

void Input_Update()
{
    // Keyboard state
    s_keyboard_state = SDL_GetKeyboardState(NULL);

    // Mouse button state
    s_mouse_buttons = SDL_GetMouseState(NULL, NULL);

    // Finalize mouse delta
    s_mouse_dx = s_mouse_accum_x;
    s_mouse_dy = s_mouse_accum_y;
    s_mouse_accum_x = 0.0f;
    s_mouse_accum_y = 0.0f;

    // Evaluate all actions
    for (int a = 0; a < ACTION_COUNT; a++)
    {
        s_actions[a].prev_pressed = s_actions[a].pressed;

        float max_val = 0.0f;
        for (int i = 0; i < s_bindings[a].count; i++)
        {
            float v = evaluate_binding(s_bindings[a].bindings[i]);
            if (v > max_val) max_val = v;
        }
        s_actions[a].value   = max_val;
        s_actions[a].pressed = (max_val > 0.0f);
    }
}

// Queries

bool Input_IsActionPressed(InputAction action)
{
    if (action >= ACTION_COUNT) return false;
    return s_actions[action].pressed;
}

bool Input_IsActionJustPressed(InputAction action)
{
    if (action >= ACTION_COUNT) return false;
    return s_actions[action].pressed && !s_actions[action].prev_pressed;
}

bool Input_IsActionJustReleased(InputAction action)
{
    if (action >= ACTION_COUNT) return false;
    return !s_actions[action].pressed && s_actions[action].prev_pressed;
}

float Input_GetActionValue(InputAction action)
{
    if (action >= ACTION_COUNT) return 0.0f;
    return s_actions[action].value;
}

void Input_GetMouseDelta(float* dx, float* dy)
{
    if (dx) *dx = s_mouse_dx;
    if (dy) *dy = s_mouse_dy;
}

bool Input_IsGamepadConnected()
{
    return s_gamepad != nullptr;
}

// Binding management

void Input_SetBinding(InputAction action, int slot, InputBinding binding)
{
    if (action >= ACTION_COUNT) return;
    if (slot < 0 || slot >= INPUT_MAX_BINDINGS_PER_ACTION) return;

    s_bindings[action].bindings[slot] = binding;
    if (slot >= s_bindings[action].count)
        s_bindings[action].count = slot + 1;
}

InputBinding Input_GetBinding(InputAction action, int slot)
{
    InputBinding empty = {};
    if (action >= ACTION_COUNT || slot < 0 || slot >= s_bindings[action].count) return empty;
    return s_bindings[action].bindings[slot];
}

int Input_GetBindingCount(InputAction action)
{
    if (action >= ACTION_COUNT) return 0;
    return s_bindings[action].count;
}

void Input_ClearBindings(InputAction action)
{
    if (action >= ACTION_COUNT) return;
    s_bindings[action].count = 0;
}

// Display names

const char* Input_GetActionName(InputAction action)
{
    if (action >= ACTION_COUNT) return "unknown";
    return s_action_names[action];
}

const char* Input_GetBindingDisplayName(const InputBinding& binding)
{
    // Thread-local buffer for returning names
    static thread_local char buf[128];

    switch (binding.source)
    {
    case BIND_KEYBOARD:
    {
        const char* name = SDL_GetScancodeName(binding.scancode);
        if (name && name[0]) return name;
        snprintf(buf, sizeof(buf), "Key %d", (int)binding.scancode);
        return buf;
    }
    case BIND_MOUSE_BUTTON:
        snprintf(buf, sizeof(buf), "Mouse %d", (int)binding.mouse_button);
        return buf;

    case BIND_GAMEPAD_BUTTON:
    {
        const char* name = SDL_GetGamepadStringForButton(binding.pad_button);
        if (name && name[0]) return name;
        snprintf(buf, sizeof(buf), "Pad Btn %d", (int)binding.pad_button);
        return buf;
    }
    case BIND_GAMEPAD_AXIS_POS:
    {
        const char* name = SDL_GetGamepadStringForAxis(binding.pad_axis);
        snprintf(buf, sizeof(buf), "%s+", name ? name : "Axis?");
        return buf;
    }
    case BIND_GAMEPAD_AXIS_NEG:
    {
        const char* name = SDL_GetGamepadStringForAxis(binding.pad_axis);
        snprintf(buf, sizeof(buf), "%s-", name ? name : "Axis?");
        return buf;
    }
    }

    return "?";
}
