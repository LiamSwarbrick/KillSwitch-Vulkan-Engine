#pragma once

#include "imgui.h"
#include "imgui_internal.h"  // DockBuilder API
#include "ecs_inspector.h"
#include "framegraph_visualizer.h"
#include "asset_browser.h"
#include "camera_panel.h"
#include "free_cam.h"
#include "renderer/debug_ui_api.h"
#include "core/components.h"

#include <array>

namespace DebugUI
{
    // State of windows in the debug UI
    struct DebugUIState
    {
        bool show_debug_ui      = true;   // F3 to toggle

        bool show_ecs_inspector = true;
        bool show_framegraph    = true;
        bool show_asset_browser = true;
        bool show_camera        = true;

        CameraMode camera_mode  = CameraMode::FreeCam;
        FreeCamState free_cam   = {};   // owned here; updated by FreeCam_Update each frame
        FPCamState fp_cam       = {};   // synced with game-owned FP cam state
        CameraInfo fp_camera    = {};   // synced with game-owned FP camera output
        bool has_fp_camera      = false;

        // Entity ID 
        uint32_t selected_entity_id = UINT32_MAX;

        FrameGraphVisualizer fg_viz;
        AssetBrowser         asset_browser;
        Asset*               debug_asset = nullptr;

        // Viewport texture for displaying the 3D scene inside ImGui
        ImTextureID viewport_imgui_tex    = 0;
        VkImageView viewport_view_cached  = VK_NULL_HANDLE;
    };
}

extern DebugUI::DebugUIState debug_ui_state;
extern ECS*                  debug_ecs_ptr;
extern Asset*                debug_asset_ptr;

namespace DebugUI
{
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
                state.show_camera        = true;
            }
        }
    }

    // Dockspace with default layout
    inline void DrawDockSpace(DebugUIState& state)
    {
        if (!state.show_debug_ui) return;

        // Cover the live 3D scene with an opaque background so only the Viewport panel shows it
        ImVec2 display_size = ImGui::GetMainViewport()->Size;
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(0.0f, 0.0f), display_size, IM_COL32(30, 30, 30, 255)
        );

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);

        // Build default layout once; skipped if imgui.ini already has a saved layout.
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        if (node && node->ChildNodes[0] == nullptr)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0, 0, 0, 0);
            style.Colors[ImGuiCol_WindowBg].w = 0.5f;  // Keep normal windows translucent

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID top, bottom;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, &bottom, &top);

            ImGuiID top_left, top_right;
            ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.25f, &top_left, &top_right);

            ImGuiID bottom_left, bottom_right;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.25f, &bottom_left, &bottom_right);

            ImGui::DockBuilderDockWindow("ECS Inspector", top_left);
            ImGui::DockBuilderDockWindow("Asset Browser", bottom_left);
            ImGui::DockBuilderDockWindow("Framegraph", bottom_right);
            ImGui::DockBuilderDockWindow("Viewport", top_right);

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

    inline void DrawCameraPanel(DebugUIState& state, ECS& ecs)
    {
        if (!state.show_debug_ui) return;

        constexpr int k_max_players = 256;
        std::array<EntityID, k_max_players> player_candidates = {};
        int player_count = 0;

        ecs.GetView<C_Transform, C_AnimatedMesh>().ForEach([&](EntityID id, C_Transform&, C_AnimatedMesh&)
        {
            if (player_count < k_max_players)
                player_candidates[player_count++] = id;
        });

        DebugUI::DrawCameraPanel(
            state.show_camera,
            state.camera_mode,
            state.free_cam,
            state.fp_cam,
            player_candidates.data(),
            player_count
        );
    }

    // Called after ImGui::NewFrame()
    inline void Draw(DebugUIState& state, ECS& ecs)
    {
        HandleInput(state);
        DrawDockSpace(state);

        // Viewport: shows the 3D scene as a texture when the debug UI is open
        if (state.show_debug_ui)
        {
            // Sync ImTextureID with the current HDR image view.
            // The view handle changes after a window resize, so we re-register in that case.
            FG_Resource& hdr_res = renderstate.registry.resources[renderstate.rids.hdr_color_target_rid];
            VkImageView cur_view = hdr_res.image.view;
            if (cur_view != state.viewport_view_cached)
            {
                if (state.viewport_imgui_tex)
                    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)state.viewport_imgui_tex);
                state.viewport_imgui_tex = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                    renderstate.heap.samplers[FG_SAMPLER_LINEAR_REPEAT],
                    cur_view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
                state.viewport_view_cached = cur_view;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            bool vp_open = ImGui::Begin("Viewport", nullptr,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();
            if (vp_open && state.viewport_imgui_tex)
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                if (avail.x > 0 && avail.y > 0)
                    ImGui::Image(state.viewport_imgui_tex, avail);
            }
            ImGui::End();
        }

        DrawECSInspector(state, ecs);
        DrawFramegraph(state);
        DrawAssetBrowser(state);
        DrawCameraPanel(state, ecs);
    }
}