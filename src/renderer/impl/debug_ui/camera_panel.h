#pragma once

#include "imgui.h"
#include "free_cam.h"   // FreeCamState

namespace DebugUI
{
    // Camera mode list — add entries here as new camera types are introduced.
    enum class CameraMode { FreeCam };
    static constexpr const char* k_camera_mode_names[] = { "Free Cam" };

    inline void DrawCameraPanel(bool& show, CameraMode& mode, const FreeCamState& cam)
    {
        if (!show) return;

        // Default position: floating bottom-right of the screen, undocked.
        ImVec2 screen = ImGui::GetMainViewport()->Size;
        constexpr ImVec2 k_panel_size(300.0f, 200.0f);
        ImGui::SetNextWindowPos(
            ImVec2(screen.x - k_panel_size.x - 16.0f, screen.y * 0.62f),
            ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowSize(k_panel_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Camera", &show, ImGuiWindowFlags_NoDocking))
        {
            ImGui::End();
            return;
        }

        // --- Mode selector ---
        ImGui::SeparatorText("Mode");
        constexpr int mode_count = (int)(sizeof(k_camera_mode_names) / sizeof(k_camera_mode_names[0]));
        int mode_int = (int)mode;
        for (int i = 0; i < mode_count; ++i)
        {
            if (ImGui::RadioButton(k_camera_mode_names[i], mode_int == i))
                mode = (CameraMode)i;
            if (i < mode_count - 1) ImGui::SameLine();
        }

        // --- Live state ---
        ImGui::SeparatorText("State");
        ImGui::Text("Position  %.2f  %.2f  %.2f", cam.pos.x,     cam.pos.y,     cam.pos.z);
        ImGui::Text("Forward   %.2f  %.2f  %.2f", cam.forward.x, cam.forward.y, cam.forward.z);
        ImGui::Text("Yaw       %.1f deg", cam.yaw);
        ImGui::Text("Pitch     %.1f deg", cam.pitch);
        ImGui::SeparatorText("Controls");
        ImGui::TextDisabled("RMB drag  look");
        ImGui::TextDisabled("WASD      fly");
        ImGui::TextDisabled("E / Q     up / down");
        ImGui::TextDisabled("Shift     sprint");

        ImGui::End();
    }
}

