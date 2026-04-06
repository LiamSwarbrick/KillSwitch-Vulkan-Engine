#pragma once

#include "imgui.h"
#include "imgui_node_editor.h"
#include "../renderer/impl/internal_state.h"  // renderstate, FrameGraph, ResourceRegistry, FG_*

namespace ed = ax::NodeEditor;

namespace DebugUI
{

struct FrameGraphVisualizer
{
    ed::EditorContext* context      = nullptr;
    bool               needs_layout = true;  // True until first Draw() with passes present

    // ---- Stable ID encoding ----
    // Node IDs:        1 .. MAX_PASSES
    // Input pin IDs:   10000 + p*16 + i          (i < 8 = MAX_PASS_RESOURCE_BANDWIDTH)
    // Output pin IDs:  10000 + p*16 + 8 + o      (o < 8)
    // Link IDs:        20000 + auto-increment per frame (stable while graph is constant)

    static ed::NodeId node_id    (uint32_t p)            { return ed::NodeId(p + 1); }
    static ed::PinId  input_pin  (uint32_t p, uint32_t i){ return ed::PinId(10000 + p * 16 + i); }
    static ed::PinId  output_pin (uint32_t p, uint32_t o){ return ed::PinId(10000 + p * 16 + 8 + o); }

    // ---- Usage-flag -> link/pin colour ----
    static ImVec4 usage_color(FG_UsageFlags flags)
    {
        if (flags & FG_USAGE_COLOR)   return ImVec4(0.20f, 0.85f, 0.30f, 1.0f);  // green
        if (flags & FG_USAGE_DEPTH)   return ImVec4(0.30f, 0.55f, 1.00f, 1.0f);  // blue
        if (flags & FG_USAGE_STENCIL) return ImVec4(0.60f, 0.30f, 1.00f, 1.0f);  // purple
        if (flags & FG_USAGE_STORAGE) return ImVec4(1.00f, 0.60f, 0.20f, 1.0f);  // orange
        if (flags & FG_USAGE_SAMPLED) return ImVec4(1.00f, 0.90f, 0.20f, 1.0f);  // yellow
        return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    }

    ~FrameGraphVisualizer() { Shutdown(); }

    void Shutdown()
    {
        if (context) { ed::DestroyEditor(context); context = nullptr; }
    }

    // Call this inside an open ImGui window every frame the window is visible.
    void Draw()
    {
        if (!context)
        {
            ed::Config cfg;
            cfg.SettingsFile = nullptr;  // Don't persist layout to disk
            context = ed::CreateEditor(&cfg);
        }

        FrameGraph*       fg  = &renderstate.framegraph;
        ResourceRegistry* reg = &renderstate.registry;

        ed::SetCurrentEditor(context);
        ed::Begin("FrameGraph Visualizer");

        // ----------------------------------------------------------------
        // Nodes  (one per render pass)
        // ----------------------------------------------------------------
        for (uint32_t p = 0; p < fg->pass_count; ++p)
        {
            RenderPassDesc* pass = &fg->passes[p];

            ed::BeginNode(node_id(p));

            // Header: colour-code by pass type
            ImVec4 header_col = pass->is_compute
                ? ImVec4(0.70f, 0.40f, 1.00f, 1.0f)   // compute  -> purple
                : ImVec4(0.40f, 0.95f, 0.55f, 1.0f);  // graphics -> green
            ImGui::PushStyleColor(ImGuiCol_Text, header_col);
            ImGui::Text("[%s] %s", pass->is_compute ? "C" : "G", pass->debug_name);
            ImGui::PopStyleColor();

            // Input pins
            for (uint32_t i = 0; i < pass->input_count; ++i)
            {
                PassResourceUsage* u = &pass->inputs[i];
                ed::BeginPin(input_pin(p, i), ed::PinKind::Input);
                ImGui::PushStyleColor(ImGuiCol_Text, usage_color(u->usage_flags));
                ImGui::Text("-> %s", reg->resources[u->rid].debug_name);
                ImGui::PopStyleColor();
                ed::EndPin();
            }

            // Output pins
            for (uint32_t o = 0; o < pass->output_count; ++o)
            {
                PassResourceUsage* u = &pass->outputs[o];
                ed::BeginPin(output_pin(p, o), ed::PinKind::Output);
                ImGui::PushStyleColor(ImGuiCol_Text, usage_color(u->usage_flags));
                ImGui::Text("%s ->", reg->resources[u->rid].debug_name);
                ImGui::PopStyleColor();
                ed::EndPin();
            }

            ed::EndNode();

            // First-open: arrange nodes in a horizontal pipeline layout
            if (needs_layout && fg->pass_count > 0)
                ed::SetNodePosition(node_id(p), ImVec2((float)p * 280.0f, 0.0f));
        }

        // ----------------------------------------------------------------
        // Links  (resource dependencies between passes)
        // ----------------------------------------------------------------
        int link_counter = 20000;
        for (uint32_t p = 0; p < fg->pass_count; ++p)
        {
            RenderPassDesc* pass = &fg->passes[p];
            for (uint32_t i = 0; i < pass->input_count; ++i)
            {
                uint32_t       rid   = pass->inputs[i].rid;
                FG_UsageFlags  flags = pass->inputs[i].usage_flags;

                // Walk backwards to find the nearest pass that writes this resource
                for (int prev = (int)p - 1; prev >= 0; --prev)
                {
                    RenderPassDesc* src = &fg->passes[prev];
                    bool found = false;
                    for (uint32_t o = 0; o < src->output_count; ++o)
                    {
                        if (src->outputs[o].rid == rid)
                        {
                            ed::LinkId lid(link_counter++);
                            ed::Link(lid,
                                     output_pin((uint32_t)prev, o),
                                     input_pin(p, i),
                                     usage_color(flags), 2.0f);
                            ed::Flow(lid);  // Animated flow dots on the link
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
        }

        // ----------------------------------------------------------------
        // First-open: fit all nodes into view
        // ----------------------------------------------------------------
        if (needs_layout && fg->pass_count > 0)
        {
            ed::NavigateToContent(0.0f);  // 0 = instant, no animation
            needs_layout = false;
        }

        ed::End();
        ed::SetCurrentEditor(nullptr);
    }

    // Call this to force a re-layout (e.g. after a scene change)
    void Reset() { needs_layout = true; }
};

}  // namespace DebugUI
