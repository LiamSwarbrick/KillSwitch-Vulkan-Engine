#pragma once

#include "imgui.h"
#include "free_cam.h"   // FreeCamState
#include "renderer/debug_ui_api.h"  // FPCamState

#include <cstdio>

namespace DebugUI
{
    static constexpr const char* k_camera_mode_names[] = { "Free Cam", "FP Cam", "TP Cam" };

    inline void DrawCameraPanel(
        bool& show,
        DebugUICameraMode& mode,
        const FreeCamState& free_cam,
        FPCamState& fp_cam,
        TPCamState& tp_cam,
        const EntityID* player_candidates,
        int player_candidate_count)
    {
        if (!show) return;

        // Default position: floating bottom-right of the screen, undocked.
        ImVec2 screen = ImGui::GetMainViewport()->Size;
        constexpr ImVec2 k_panel_size(340.0f, 300.0f);
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
                mode = (DebugUICameraMode)i;
            if (i < mode_count - 1) ImGui::SameLine();
        }

        auto DrawBoundPlayerCombo = [&](const char* label, EntityID& bound_entity)
        {
            bool has_players = (player_candidate_count > 0 && player_candidates);
            if (!has_players)
                bound_entity = NULL_ENTITY;

            char preview[64];
            if (has_players)
            {
                if (bound_entity == NULL_ENTITY)
                    bound_entity = player_candidates[0];

                snprintf(preview, sizeof(preview), "Entity %u", bound_entity);
            }
            else
            {
                snprintf(preview, sizeof(preview), "<None>");
            }

            if (ImGui::BeginCombo(label, preview))
            {
                if (has_players)
                {
                    for (int i = 0; i < player_candidate_count; ++i)
                    {
                        EntityID id = player_candidates[i];
                        char item_label[64];
                        snprintf(item_label, sizeof(item_label), "Entity %u", id);

                        bool selected = (bound_entity == id);
                        if (ImGui::Selectable(item_label, selected))
                            bound_entity = id;
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                else
                {
                    ImGui::TextDisabled("No player entities");
                }
                ImGui::EndCombo();
            }
        };

        // --- Live state ---
        ImGui::SeparatorText("State");
        switch (mode)
        {
            case DebugUICameraMode::FreeCam:
            {
                ImGui::Text("Position  %.2f  %.2f  %.2f", free_cam.pos.x,     free_cam.pos.y,     free_cam.pos.z);
                ImGui::Text("Forward   %.2f  %.2f  %.2f", free_cam.forward.x, free_cam.forward.y, free_cam.forward.z);
                ImGui::Text("Yaw       %.1f deg", free_cam.yaw);
                ImGui::Text("Pitch     %.1f deg", free_cam.pitch);

                ImGui::SeparatorText("Controls");
                ImGui::TextDisabled("RMB drag  look");
                ImGui::TextDisabled("WASD      fly");
                ImGui::TextDisabled("E / Q     up / down");
                ImGui::TextDisabled("Shift     sprint");
                break;
            }

            case DebugUICameraMode::FPCam:
            {
                ImGui::Text("Position  %.2f  %.2f  %.2f", fp_cam.pos.x,     fp_cam.pos.y,     fp_cam.pos.z);
                ImGui::Text("Forward   %.2f  %.2f  %.2f", fp_cam.forward.x, fp_cam.forward.y, fp_cam.forward.z);
                ImGui::Text("Yaw       %.1f deg", fp_cam.yaw);
                ImGui::Text("Pitch     %.1f deg", fp_cam.pitch);

                DrawBoundPlayerCombo("Bound Player", fp_cam.bound_entity);

                if (ImGui::SliderFloat("FOV", &fp_cam.fov_deg, 40.0f, 4000.0f, "%.1f deg"))
                {
                    fp_cam.fov_initialized = true;
                }

                if (ImGui::Button("Reset FP Cam"))
                {
                    EntityID keep_bound = fp_cam.bound_entity;
                    FPCam_ResetToDefault(fp_cam);
                    fp_cam.bound_entity = keep_bound;
                }

                ImGui::SeparatorText("Controls");
                ImGui::TextDisabled("Mouse / Right-stick  look");
                ImGui::TextDisabled("Bind Player dropdown");
                ImGui::TextDisabled("FOV + Reset affect FP cam");
                break;
            }

            case DebugUICameraMode::TPCam:
            {
                ImGui::Text("Position  %.2f  %.2f  %.2f", tp_cam.pos.x,     tp_cam.pos.y,     tp_cam.pos.z);
                ImGui::Text("Target    %.2f  %.2f  %.2f", tp_cam.target.x,  tp_cam.target.y,  tp_cam.target.z);
                ImGui::Text("Yaw       %.1f deg", tp_cam.yaw);
                ImGui::Text("Pitch     %.1f deg", tp_cam.pitch);

                DrawBoundPlayerCombo("Bound Player", tp_cam.bound_entity);

                if (ImGui::SliderFloat("Distance", &tp_cam.distance, 1.0f, 12.0f, "%.2f m"))
                {
                    if (tp_cam.distance < 0.5f)
                        tp_cam.distance = 0.5f;
                }

                if (ImGui::SliderFloat("FOV", &tp_cam.fov_deg, 40.0f, 4000.0f, "%.1f deg"))
                {
                    tp_cam.fov_initialized = true;
                }

                if (ImGui::Button("Reset TP Cam"))
                {
                    EntityID keep_bound = tp_cam.bound_entity;
                    TPCam_ResetToDefault(tp_cam);
                    tp_cam.bound_entity = keep_bound;
                }

                ImGui::SeparatorText("Controls");
                ImGui::TextDisabled("Mouse / Right-stick  orbit");
                ImGui::TextDisabled("Distance + FOV sliders");
                ImGui::TextDisabled("Reset restores TP defaults");
                break;
            }

            default:
                // Placeholder for future modes (e.g. TPCam).
                ImGui::TextDisabled("Camera mode not implemented yet.");
                break;
        }

        ImGui::End();
    }
}

