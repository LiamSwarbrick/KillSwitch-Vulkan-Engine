#pragma once

#include "imgui.h"
#include "../imgui-node-editor/imgui_node_editor.h"
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
            ImGui::Text("[%s] %s", pass->is_compute ? "COMP" : "GFX", pass->debug_name);
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
                ed::SetNodePosition(node_id(p), ImVec2((float)p * 280.0f, (float)p * 100.0f));
        }

        // ----------------------------------------------------------------
        // Links  (resource dependencies between passes)
        // ----------------------------------------------------------------
        int link_counter = 20000;
        for (uint32_t p = 0; p < fg->pass_count; ++p)
        {
            RenderPassDesc* pass = &fg->passes[p];

            // Check sampled textures: Inputs are textures, see if they are outputs of a prev pass
            for (uint32_t i = 0; i < pass->input_count; ++i)
            {
                uint32_t target_rid   = pass->inputs[i].rid;
                FG_UsageFlags flags   = pass->inputs[i].usage_flags;

                // Walk backwards to find the nearest pass that writes this resource
                for (int prev = (int)p - 1; prev >= 0; --prev)
                {
                    RenderPassDesc* src = &fg->passes[prev];
                    bool found = false;
                    for (uint32_t o = 0; o < src->output_count; ++o)
                    {
                        // Check either this rid or it's resolve (cuz MSAA means it uses a resolve target instead)
                        bool attachment_is_target = src->outputs[o].rid == target_rid;
                        bool is_resolve = (src->outputs[o].resolve_rid == target_rid)
                            && !attachment_is_target;  // <- Account for the fact that resources are aliased when MSAA is off,
                            // and we wanna make sure we don't show this as being a 'resolve' step when it's not resolving anything!
                        if (attachment_is_target || is_resolve)
                        {
                            ed::LinkId lid(link_counter++);
                            ed::Link(lid,
                                     output_pin((uint32_t)prev, o),
                                     input_pin(p, i),
                                     usage_color(flags), 2.0f);
                            if (is_resolve)
                            {
                                // Only show a flowy line when MSAA resolve is happening between these pass usages (indicates the extra work happening) 
                                ed::Flow(lid);  // Animated flow dots on the link
                            }
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }

            // Outputs are attachments, i.e. an attachment may also be used again...
            for (uint32_t o_curr = 0; o_curr < pass->output_count; ++o_curr)
            {
                uint32_t target_rid = pass->outputs[o_curr].rid; 

                // We only care about linking outputs if the current pass is LOADING (reusing) it
                if (pass->outputs[o_curr].load_op == VK_ATTACHMENT_LOAD_OP_LOAD)
                {
                    for (int prev = (int)p - 1; prev >= 0; --prev)
                    {
                        RenderPassDesc* src = &fg->passes[prev];
                        bool found = false;
                        for (uint32_t o_prev = 0; o_prev < src->output_count; ++o_prev)
                        {
                            bool attachment_is_target = src->outputs[o_prev].rid == target_rid;
                            bool is_resolve = (src->outputs[o_prev].resolve_rid == target_rid)
                                && !attachment_is_target;
                            if (attachment_is_target || is_resolve)
                            {
                                // LINK: Prev Output Pin -> Current Output Pin
                                ed::LinkId lid(link_counter++);
                                ed::Link(lid, 
                                        output_pin(prev, o_prev), 
                                        output_pin(p, o_curr), 
                                        usage_color(pass->outputs[o_curr].usage_flags), 2.0f);
                                if (is_resolve)
                                {
                                    ed::Flow(lid);
                                }
                                found = true; break;
                            }
                        }
                        if (found) break;
                    }
                }
            }            
        }

        // ----------------------------------------------------------------
        // First-open: fit all nodes into view
        // ----------------------------------------------------------------
        if (true)  // TODO: <- This was if (needs_layout) but initialization not working
        {
            ed::NavigateToContent(0.0f);  // 0 = instant, no animation
            needs_layout = false;
        }

        ed::End();
        ed::SetCurrentEditor(nullptr);

        // ----------------------------------------------------------------
        // Legend / Key (top-right overlay)
        // ----------------------------------------------------------------
        {
            ImVec2 top_right = ImGui::GetWindowPos();
            top_right.x += ImGui::GetWindowSize().x * 0.75;
            top_right.y += 20.0f;

            ImGui::SetNextWindowPos(top_right);
            ImGui::SetNextWindowBgAlpha(0.90f);

            ImGui::Begin("FG Key",
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav);

            ImGui::Text("FrameGraph Key");
            ImGui::Separator();

            ImGui::Text("Colour = Resource Usage");

            ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.30f, 1.0f), "- Color Attachment");
            ImGui::TextColored(ImVec4(0.30f, 0.55f, 1.00f, 1.0f), "- Depth Attachment");
            ImGui::TextColored(ImVec4(0.60f, 0.30f, 1.00f, 1.0f), "- Stencil Attachment");
            ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.0f), "- Storage");
            ImGui::TextColored(ImVec4(1.00f, 0.90f, 0.20f, 1.0f), "- Sampled");

            ImGui::Spacing();

            ImGui::Text("Link Behaviour");
            ImGui::Text("- Static   = Regular Dependency");
            ImGui::Text("- Flowing  = MSAA Resolve between nodes");

            // NOTE: For now I'm adding debug rendermode toggles here
            bool clustered_heatmap = renderstate.debug_rendermode == DEBUG_RENDERMODE_CLUSTERED_SHADING_HEATMAP;
            bool cluster_count_mode = renderstate.debug_rendermode == DEBUG_RENDERMODE_CLUSTERED_SHADING_CLUSTERS;
            bool unlit_mode = renderstate.debug_rendermode == DEBUG_RENDERMODE_UNLIT;
            ImGui::Checkbox("Clustered shading heatmap", &clustered_heatmap);
            ImGui::Checkbox("Visualize clusters", &cluster_count_mode);
            ImGui::Checkbox("Unlit materials", &unlit_mode);
            if (clustered_heatmap)
            {
                renderstate.debug_rendermode = DEBUG_RENDERMODE_CLUSTERED_SHADING_HEATMAP;
            }

            if (cluster_count_mode)
            {
                renderstate.debug_rendermode = DEBUG_RENDERMODE_CLUSTERED_SHADING_CLUSTERS;
            }

            if (unlit_mode)
            {
                renderstate.debug_rendermode = DEBUG_RENDERMODE_UNLIT;
            }

            ImGui::End();
        }
    }

    // Call this to force a re-layout (e.g. after a scene change)
    void Reset() { needs_layout = true; }
};

}  // namespace DebugUI
