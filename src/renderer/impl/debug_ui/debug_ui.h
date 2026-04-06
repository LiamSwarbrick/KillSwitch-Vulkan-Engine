#pragma once

#include "imgui.h"
#include "ecs_inspector.h"
#include "framegraph_visualizer.h"
#include "asset_browser.h"


namespace DebugUI
{
    // State of menubar and windows in the debug UI
    struct DebugUIState
    {
        bool show_debug_ui      = false;  // F3 to toggle

        bool show_ecs_inspector = false;
        bool show_framegraph    = false;
        bool show_asset_browser = false;

        // Entity ID 
        uint32_t selected_entity_id = UINT32_MAX;

        #warning Waiting for node editor from Nansong
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

    // Full-screen transparent dockspace so windows can be docked anywhere.
    inline void DrawDockSpace(DebugUIState& state)
    {
        if (!state.show_debug_ui) return;
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    inline void DrawECSInspector(DebugUIState& state, AdvEng::ECS& ecs)
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
    inline void Draw(DebugUIState& state, AdvEng::ECS& ecs)
    {
        HandleInput(state);
        DrawDockSpace(state);
        DrawECSInspector(state, ecs);
        DrawFramegraph(state);
        DrawAssetBrowser(state);
    }
}