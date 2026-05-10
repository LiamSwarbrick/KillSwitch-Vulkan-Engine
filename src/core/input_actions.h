#ifndef CORE_INPUT_ACTIONS_H
#define CORE_INPUT_ACTIONS_H

#include <stdint.h>


//  To add a new action:
//    1. Add a row here
//    2. Add default bindings in set_default_bindings() in input.cpp

#define INPUT_ACTIONS_LIST \
    /* Movement */         \
    X(MOVE_FORWARD,  "move_forward")  \
    X(MOVE_BACKWARD, "move_backward") \
    X(MOVE_LEFT,     "move_left")     \
    X(MOVE_RIGHT,    "move_right")    \
    X(MOVE_UP,       "move_up")       \
    X(MOVE_DOWN,     "move_down")     \
    X(SPRINT,        "sprint")        \
    X(JUMP,          "jump")          \
    X(CROUCH,        "crouch")        \
    /* Camera */           \
    X(CAMERA_UP,     "camera_up")     \
    X(CAMERA_DOWN,   "camera_down")   \
    X(CAMERA_LEFT,   "camera_left")   \
    X(CAMERA_RIGHT,  "camera_right")  \
    X(TOGGLE_CAMERA, "toggle_camera") \
    /* Gameplay */         \
    X(INTERACT,      "interact")      \
    X(ATTACK,        "attack")        \
    X(AIM,           "aim")           \
    X(RELOAD,        "reload")        \
    /* UI / System */      \
    X(PAUSE,         "pause")         \
    X(DEBUG_TOGGLE,  "debug_toggle")

// Generate the enum from the list above
enum InputAction : uint32_t
{
#define X(suffix, name) ACTION_##suffix,
    INPUT_ACTIONS_LIST
#undef X
    ACTION_COUNT  // must always be last
};

#endif // CORE_INPUT_ACTIONS_H
