#ifndef DEBUG_UI_API_H
#define DEBUG_UI_API_H

#include "core/ecs.h"
#include "core/assetsys.h"
#include "renderer/renderer.h"   // CameraInfo

void DebugUI_SetECS(ECS* ecs);
void DebugUI_SetAsset(Asset* asset);

// Optional callback: called between ImGui::NewFrame() and ImGui::Render()
// Game code can set this to build its own ImGui UI
typedef void (*DebugUI_ImGuiBuildCallback)(void* user_data);
void DebugUI_SetImGuiCallback(DebugUI_ImGuiBuildCallback callback, void* user_data);

bool DebugUI_IsOpen();

// Update the debug free-cam for this frame and return a CameraInfo.
// Call each frame from main.cpp and forward to Renderer_DrawFrame.
CameraInfo DebugUI_GetCameraInfo(float dt);

#endif  // DEBUG_UI_API_H
