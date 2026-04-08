#pragma once

#include "imgui.h"
#include "imgui_internal.h"  // DockBuilder API
#include "ecs_inspector.h"
#include "framegraph_visualizer.h"
#include "asset_browser.h"


namespace DebugUI
{
    // State of windows in the debug UI
    struct DebugUIState
    {
        bool show_debug_ui      = false;  // F3 to toggle

        bool show_ecs_inspector = false;
        bool show_framegraph    = false;
        bool show_asset_browser = false;

        // Entity ID 
        uint32_t selected_entity_id = UINT32_MAX;

        FrameGraphVisualizer fg_viz;
        AssetBrowser         asset_browser;
        Asset*               debug_asset = nullptr;
    };

    inline void HandleInput(DebugUIState& state)
    {
        // F3: toggle debug UI; opening restores all three panels
        if (ImGui::IsKeyPressed(ImGuiKey_F3))
        {
            state.show_debug_ui = !state.show_debug_ui;
            if (state.show_debug_ui)
            {
                state.show_ecs_inspector = true;
                state.show_framegraph    = true;
                state.show_asset_browser = true;
            }
        }
    }

    // Dockspace with default layout
    inline void DrawDockSpace(DebugUIState& state)
    {
        if (!state.show_debug_ui) return;
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // Build default layout once; skipped if imgui.ini already has a saved layout.
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        if (node && node->IsLeafNode())
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            // Split off bottom strip (30% height) — full width
            ImGuiID top, bottom;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, &bottom, &top);

            // Split top: ECS on the left (30% of total width), game viewport on the right (passthru)
            ImGuiID top_left, top_right;
            ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.30f, &top_left, &top_right);

            // Split bottom strip 50/50
            ImGuiID bottom_left, bottom_right;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.50f, &bottom_left, &bottom_right);

            ImGui::DockBuilderDockWindow("ECS Inspector",  top_left);
            ImGui::DockBuilderDockWindow("Asset Browser",  bottom_left);
            ImGui::DockBuilderDockWindow("Framegraph",     bottom_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    inline void DrawECSInspector(DebugUIState& state, ECS& ecs)
    {
        if (!state.show_debug_ui || !state.show_ecs_inspector) return;

        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("ECS Inspector", &state.show_ecs_inspector))
        {
            ImGui::End();
            return;
        }

        DrawECSInspectorContent(ecs, state.selected_entity_id);  

        ImGui::End();
    }

    inline void DrawFramegraph(DebugUIState& state)
    {
        if (!state.show_debug_ui || !state.show_framegraph) return;
        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Framegraph", &state.show_framegraph))
        {
            #warning NOTE(Liam), See warnings in debug_ui. Commenting out node editor until Nansong sends his working build of it to the repo.
            state.fg_viz.Draw();
        }
        ImGui::End();
    }

    inline void DrawAssetBrowser(DebugUIState& state)
    {
        if (!state.show_debug_ui || !state.show_asset_browser) return;
        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Asset Browser", &state.show_asset_browser))
        {
            state.asset_browser.Draw(state.debug_asset);
        }
        ImGui::End();
    }

    // Called after ImGui::NewFrame()
    inline void Draw(DebugUIState& state, ECS& ecs)
    {
        HandleInput(state);
        DrawDockSpace(state);
        DrawECSInspector(state, ecs);
        DrawFramegraph(state);
        DrawAssetBrowser(state);
    }
}