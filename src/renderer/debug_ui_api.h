#ifndef DEBUG_UI_API_H
#define DEBUG_UI_API_H

#include "core/ecs.h"
#include "core/assetsys.h"
#include "glm/glm.hpp"

// Camera state readable from the debug UI
struct FreeCamState
{
    glm::vec3 pos     = glm::vec3(0.0f, 0.0f, 3.0f);
    float     yaw     = -90.0f;
    float     pitch   =   0.0f;
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
};

// Set by game code each frame so the debug UI can read live camera state
void                    DebugUI_SetFreeCamState(const FreeCamState* state);
const FreeCamState*     DebugUI_GetFreeCamState();

void DebugUI_SetECS(ECS* ecs);
void DebugUI_SetAsset(Asset* asset);

// Optional callback: called between ImGui::NewFrame() and ImGui::Render()
// Game code can set this to build its own ImGui UI
typedef void (*DebugUI_ImGuiBuildCallback)(void* user_data);
void DebugUI_SetImGuiCallback(DebugUI_ImGuiBuildCallback callback, void* user_data);

bool DebugUI_IsOpen();

#endif  // DEBUG_UI_API_H
