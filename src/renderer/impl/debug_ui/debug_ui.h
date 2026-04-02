#pragma once

#include "imgui.h"
#include "ecs_inspector.h"


namespace DebugUI
{
    struct DebugUIState
    {
        bool show_debug_ui      = false;  // F3 to toggle

        bool show_ecs_inspector = false;
        bool show_framegraph    = false;
        bool show_asset_browser = false;
        uint32_t selected_entity_id = UINT32_MAX;
    };

    inline void HandleInput(DebugUIState& state)
    {
        // F3 to toggle the entire Debug UI
        if (ImGui::IsKeyPressed(ImGuiKey_F3))
        {
            state.show_debug_ui = !state.show_debug_ui;
        }
    }

    inline void DrawMainMenuBar(DebugUIState& state)
    {
        if (!state.show_debug_ui) return;

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Debug"))
            {
                ImGui::MenuItem("ECS Inspector",  nullptr, &state.show_ecs_inspector);
                ImGui::MenuItem("Framegraph",     nullptr, &state.show_framegraph);
                ImGui::MenuItem("Asset Browser",  nullptr, &state.show_asset_browser);
                ImGui::Separator();
                ImGui::TextDisabled("F3 to toggle UI");
                ImGui::EndMenu();
            }
            // Display hint on the right side
            ImGui::SetCursorPosX(ImGui::GetIO().DisplaySize.x - 100.0f);
            ImGui::TextDisabled("[F3] Debug UI");
            ImGui::EndMainMenuBar();
        }
    }

    inline void DrawECSInspector(DebugUIState& state)
    {
        if (!state.show_debug_ui || !state.show_ecs_inspector) return;

        ImGui::SetNextWindowSize(ImVec2(900, 900), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("ECS Inspector", &state.show_ecs_inspector))
        {
            ImGui::End();
            return;
        }

        DrawECSInspectorContent(state.selected_entity_id);  

        ImGui::End();
    }

    inline void DrawFramegraph(DebugUIState& state)
    {
        if (!state.show_debug_ui || !state.show_framegraph) return;
        if (ImGui::Begin("Framegraph", &state.show_framegraph))
        {
            ImGui::TextDisabled("(Framegraph Visualizer - TODO)");
        }
        ImGui::End();
    }

    inline void DrawAssetBrowser(DebugUIState& state)
    {
        if (!state.show_debug_ui || !state.show_asset_browser) return;
        if (ImGui::Begin("Asset Browser", &state.show_asset_browser))
        {
            ImGui::TextDisabled("(Asset Browser - TODO)");
        }
        ImGui::End();
    }

    // Called after ImGui::NewFrame()
    inline void Draw(DebugUIState& state)
    {
        HandleInput(state);
        DrawMainMenuBar(state);
        DrawECSInspector(state);
        DrawFramegraph(state);
        DrawAssetBrowser(state);
    }
}