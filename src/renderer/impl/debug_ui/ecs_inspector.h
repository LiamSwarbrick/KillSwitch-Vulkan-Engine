#pragma once

#include "imgui.h"
#include <vector>
#include <cstdint>

namespace DebugUI
{
    // placeholder
    struct MockComponent
    {
        const char* name;
    };

    struct MockEntity
    {
        uint32_t                   id;
        const char*                tag;
        std::vector<MockComponent> components;
        std::vector<MockEntity>    children;
    };

    inline std::vector<MockEntity> MakeMockEntityTree()
    {
        return {
            { 0, "Player", {{"C_Transform"}, {"C_AnimatedMesh"}}, {
                { 2, "Sword",  {{"C_Transform"}}, {} },
                { 3, "Shield", {{"C_Transform"}}, {} },
            }},
            { 1, "DirectionalLight", {{"C_Transform"}}, {} },
            { 4, "Level_Root", {{"C_Transform"}}, {
                { 5, "Floor",  {{"C_Transform"}}, {} },
                { 6, "Wall_A", {{"C_Transform"}}, {} },
                { 7, "Wall_B", {{"C_Transform"}}, {} },
            }},
        };
    }
    // ---------------------------------------------------------------

    inline void DrawEntityNode(const MockEntity& entity, uint32_t& selected_id)
    {
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (entity.children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected_id == entity.id)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(
            (void*)(uintptr_t)entity.id,
            flags,
            "[%u] %s", entity.id, entity.tag
        );

        if (ImGui::IsItemClicked())
            selected_id = entity.id;

        if (open)
        {
            for (const auto& child : entity.children)
                DrawEntityNode(child, selected_id);
            ImGui::TreePop();
        }
    }

    inline void DrawComponentPanel(const MockEntity* entity)
    {
        if (!entity)
        {
            ImGui::TextDisabled("Select an entity to inspect.");
            return;
        }

        ImGui::Text("[%u] %s", entity->id, entity->tag);
        ImGui::Separator();
        ImGui::TextDisabled("Components (%zu)", entity->components.size());
        ImGui::Spacing();

        for (const auto& comp : entity->components)
        {
            if (ImGui::CollapsingHeader(comp.name))
            {
                ImGui::Indent();
                ImGui::TextDisabled("(fields - TODO)");
                ImGui::Unindent();
            }
        }
    }

    inline const MockEntity* FindEntity(const std::vector<MockEntity>& entities, uint32_t id)
    {
        for (const auto& e : entities)
        {
            if (e.id == id) return &e;
            if (auto* found = FindEntity(e.children, id))
                return found;
        }
        return nullptr;
    }

    inline void DrawECSInspectorContent(uint32_t& selected_entity_id)
    {
        static auto mock_tree = MakeMockEntityTree();

        // hierarchy tree
        ImGui::BeginChild("##entity_hierarchy",
            ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, 0),
            ImGuiChildFlags_Borders
        );
        ImGui::TextDisabled("Hierarchy");
        ImGui::Separator();
        for (const auto& entity : mock_tree)
            DrawEntityNode(entity, selected_entity_id);
        ImGui::EndChild();

        ImGui::SameLine();

        // component panel
        ImGui::BeginChild("##component_panel",
            ImVec2(0, 0),
            ImGuiChildFlags_Borders
        );
        ImGui::TextDisabled("Inspector");
        ImGui::Separator();
        DrawComponentPanel(FindEntity(mock_tree, selected_entity_id));
        ImGui::EndChild();
    }
}