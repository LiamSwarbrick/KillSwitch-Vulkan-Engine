#pragma once

#include "imgui.h"
#include "free_cam.h"   // FreeCamState
#include "renderer/debug_ui_api.h"  // FPCamState

#include <cstdio>

namespace DebugUI
{
    static constexpr const char* k_camera_mode_names[] = { "Free Cam", "FP Cam", "TP Cam" };

    struct CameraPanelResult
    {
        bool fp_state_changed = false;
        bool tp_state_changed = false;
        bool reset_fp = false;
        bool reset_tp = false;
        FPCamState fp_state = {};
        TPCamState tp_state = {};
    };

    inline CameraPanelResult DrawCameraPanel(
        bool& show,
        DebugUICameraMode& mode,
        const FreeCamState& free_cam,
        const FPCamState& fp_cam,
        const TPCamState& tp_cam,
        const EntityID* player_candidates,
        int player_candidate_count)
    {
        CameraPanelResult result = {};
        FPCamState edit_fp_cam = fp_cam;
        TPCamState edit_tp_cam = tp_cam;

        if (!show)
        {
            result.fp_state = edit_fp_cam;
            result.tp_state = edit_tp_cam;
            return result;
        }

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
            result.fp_state = edit_fp_cam;
            result.tp_state = edit_tp_cam;
            return result;
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

        auto DrawBoundPlayerCombo = [&](const char* label, EntityID& bound_entity) -> bool
        {
            bool changed = false;
            bool has_players = (player_candidate_count > 0 && player_candidates);
            if (!has_players)
            {
                if (bound_entity != NULL_ENTITY)
                {
                    bound_entity = NULL_ENTITY;
                    changed = true;
                }
            }

            char preview[64];
            if (has_players)
            {
                if (bound_entity == NULL_ENTITY)
                {
                    bound_entity = player_candidates[0];
                    changed = true;
                }

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
                        {
                            bound_entity = id;
                            changed = true;
                        }
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

            return changed;
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
                ImGui::Text("Position  %.2f  %.2f  %.2f", edit_fp_cam.pos.x,     edit_fp_cam.pos.y,     edit_fp_cam.pos.z);
                ImGui::Text("Forward   %.2f  %.2f  %.2f", edit_fp_cam.forward.x, edit_fp_cam.forward.y, edit_fp_cam.forward.z);
                ImGui::Text("Yaw       %.1f deg", edit_fp_cam.yaw);
                ImGui::Text("Pitch     %.1f deg", edit_fp_cam.pitch);

                if (DrawBoundPlayerCombo("Bound Player", edit_fp_cam.bound_entity))
                    result.fp_state_changed = true;

                if (ImGui::SliderFloat("FOV", &edit_fp_cam.fov_deg, 40.0f, 4000.0f, "%.1f deg"))
                {
                    edit_fp_cam.fov_initialized = true;
                    result.fp_state_changed = true;
                }

                if (ImGui::Button("Reset FP Cam"))
                {
                    EntityID keep_bound = edit_fp_cam.bound_entity;
                    FPCam_ResetToDefault(edit_fp_cam);
                    edit_fp_cam.bound_entity = keep_bound;
                    result.reset_fp = true;
                    result.fp_state_changed = true;
                }

                ImGui::SeparatorText("Controls");
                ImGui::TextDisabled("Mouse / Right-stick  look");
                ImGui::TextDisabled("Bind Player dropdown");
                ImGui::TextDisabled("FOV + Reset affect FP cam");
                break;
            }

            case DebugUICameraMode::TPCam:
            {
                ImGui::Text("Position  %.2f  %.2f  %.2f", edit_tp_cam.pos.x,     edit_tp_cam.pos.y,     edit_tp_cam.pos.z);
                ImGui::Text("Target    %.2f  %.2f  %.2f", edit_tp_cam.target.x,  edit_tp_cam.target.y,  edit_tp_cam.target.z);
                ImGui::Text("Yaw       %.1f deg", edit_tp_cam.yaw);
                ImGui::Text("Pitch     %.1f deg", edit_tp_cam.pitch);

                if (DrawBoundPlayerCombo("Bound Player", edit_tp_cam.bound_entity))
                    result.tp_state_changed = true;

                if (ImGui::SliderFloat("Distance", &edit_tp_cam.distance, 1.0f, 12.0f, "%.2f m"))
                {
                    if (edit_tp_cam.distance < 0.5f)
                        edit_tp_cam.distance = 0.5f;
                    result.tp_state_changed = true;
                }

                if (ImGui::SliderFloat("FOV", &edit_tp_cam.fov_deg, 40.0f, 4000.0f, "%.1f deg"))
                {
                    edit_tp_cam.fov_initialized = true;
                    result.tp_state_changed = true;
                }

                if (ImGui::Button("Reset TP Cam"))
                {
                    EntityID keep_bound = edit_tp_cam.bound_entity;
                    TPCam_ResetToDefault(edit_tp_cam);
                    edit_tp_cam.bound_entity = keep_bound;
                    result.reset_tp = true;
                    result.tp_state_changed = true;
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

        result.fp_state = edit_fp_cam;
        result.tp_state = edit_tp_cam;
        return result;
    }
}

