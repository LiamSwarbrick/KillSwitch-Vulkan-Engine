#pragma once

#include "imgui.h"
#include "core/ecs.h"

#include <vector>
#include <cstdint>

namespace DebugUI
{
    inline void DrawEntityList(AdvEng::ECS& ecs, uint32_t& selected_id)
    {
        ImGui::TextDisabled("Entities (%zu)", ecs.GetEntityCount());
        ImGui::Separator();

        for (AdvEng::EntityID id : ecs.GetAllEntities())
        {
            std::string tag  = ecs.GetEntityTag(id);
            std::string label = "[" + std::to_string(id) + "] " + tag;

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf           |
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (selected_id == id)
                flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx((void*)(uintptr_t)id, flags, "%s", label.c_str());

            if (ImGui::IsItemClicked())
                selected_id = id;
        }
    }

    inline void DrawComponentMaskPanel(AdvEng::ECS& ecs, uint32_t selected_id)
    {
        if (selected_id == UINT32_MAX || !ecs.IsEntityValid(selected_id))
        {
            ImGui::TextDisabled("Select an entity to inspect.");
            return;
        }

        std::string tag = ecs.GetEntityTag(selected_id);
        ImGui::Text("[%u] %s", selected_id, tag.c_str());
        ImGui::Separator();

        AdvEng::ComponentMask mask = ecs.GetEntityComponentMask(selected_id);

        size_t pool_count   = ecs.GetPoolCount();
        size_t comp_count   = mask.count();

        ImGui::TextDisabled("Components (%zu)", comp_count);
        ImGui::Spacing();

        if (comp_count == 0)
        {
            ImGui::TextDisabled("No components.");
            return;
        }

        // get bit from bitmask
        for (size_t i = 0; i < pool_count; i++)
        {
            if (!mask[i]) continue;

            // show bit index name
            // TODO: waiting m_componentNames 
            std::string label = "Component [bit " + std::to_string(i) + "]";

            if (ImGui::CollapsingHeader(label.c_str()))
            {
                ImGui::Indent();
                ImGui::TextDisabled("(fields - TODO)");
                ImGui::Unindent();
            }
        }
    }

    inline void DrawECSInspectorContent(AdvEng::ECS& ecs, uint32_t& selected_entity_id)
    {
        // entity hierarchy on the left
        ImGui::BeginChild("##entity_hierarchy",
            ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, 0),
            ImGuiChildFlags_Borders
        );
        DrawEntityList(ecs, selected_entity_id);
        ImGui::EndChild();

        ImGui::SameLine();

        //  ComponentMask panel
        ImGui::BeginChild("##component_panel",
            ImVec2(0, 0),
            ImGuiChildFlags_Borders
        );
        ImGui::TextDisabled("Inspector");
        ImGui::Separator();
        DrawComponentMaskPanel(ecs, selected_entity_id);
        ImGui::EndChild();
    }
}