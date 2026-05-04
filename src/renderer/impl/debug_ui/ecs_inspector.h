#pragma once

#include "imgui.h"
#include "core/ecs.h"
#include "game/foundations/components.h"

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace DebugUI
{
    // field drawing functions for specific component types
    template <typename T>
    inline void DrawComponentFields(T& comp)
    {
        ImGui::TextDisabled("(no display defined for this component)");
    }

    template <>
    inline void DrawComponentFields<C_Transform>(C_Transform& t)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(t.matrix, scale, rotation, translation, skew, perspective);
        ImGui::Text("Position:  %.3f  %.3f  %.3f", translation.x, translation.y, translation.z);
        ImGui::Text("Rotation:  %.3f  %.3f  %.3f  %.3f", rotation.x, rotation.y, rotation.z, rotation.w);
        ImGui::Text("Scale:  %.3f  %.3f  %.3f", scale.x, scale.y, scale.z);
        ImGui::Text("Skew:  %.3f  %.3f  %.3f", skew.x, skew.y, skew.z);
        ImGui::Text("Perspective:  %.3f  %.3f  %.3f  %.3f", perspective.x, perspective.y, perspective.z, perspective.w);
    }

    template <>
    inline void DrawComponentFields<C_StaticMesh>(C_StaticMesh& m)
    {
        ImGui::Text("Mesh ptr:      %p", (void*)m.mesh);
        ImGui::Text("Parent asset:  %p", (void*)m.parent_asset);
    }

    template <>
    inline void DrawComponentFields<C_RigidBody>(C_RigidBody& c)
    {
        // To visualize the rigidbody we would either define the function using RigidBody 
        // or somehow have access to the Scene / PhysicsManager to get the RigidBody here
        ImGui::Text("RigidBodyHandle: %p", c.handle.index);
    }

    // Registry: bit_index → draw function
    using DrawFieldsFunc = std::function<void(ECS&, EntityID)>;

    struct ComponentDrawEntry
    {
        size_t         bit_index;
        DrawFieldsFunc draw;
    };

    template <typename T>
    inline ComponentDrawEntry MakeDrawEntry(ECS& ecs)
    {
        return {
            ecs.GetComponentBitIndex<T>(),
            [](ECS& ecs, EntityID id)
            {
                T* comp = ecs.GetComponentPtr<T>(id);
                if (comp) DrawComponentFields<T>(*comp);
            }
        };
    }

    inline void DrawEntityList(ECS& ecs, uint32_t& selected_id)
    {
        ImGui::TextDisabled("Entities (%zu)", ecs.GetEntityCount());
        ImGui::Separator();

        for (EntityID id : ecs.GetAllEntities())
        {
            std::string tag   = ecs.GetEntityTag(id);
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

    inline void DrawComponentMaskPanel(
        ECS& ecs,
        uint32_t selected_id,
        const std::vector<ComponentDrawEntry>& draw_entries)
    {
        if (selected_id == UINT32_MAX || !ecs.IsEntityValid(selected_id))
        {
            ImGui::TextDisabled("Select an entity to inspect.");
            return;
        }

        std::string tag = ecs.GetEntityTag(selected_id);
        ImGui::Text("[%u] %s", selected_id, tag.c_str());
        ImGui::Separator();

        ComponentMask mask       = ecs.GetEntityComponentMask(selected_id);
        size_t                comp_count = mask.count();

        ImGui::TextDisabled("Components (%zu)", comp_count);
        ImGui::Spacing();

        if (comp_count == 0)
        {
            ImGui::TextDisabled("No components.");
            return;
        }

        for (const auto& entry : draw_entries)
        {
            if (!mask[entry.bit_index]) continue;

            std::string name = ecs.GetComponentName(entry.bit_index);
            if (ImGui::CollapsingHeader(name.c_str()))
            {
                ImGui::Indent();
                entry.draw(ecs, selected_id);
                ImGui::Unindent();
            }
        }
    }

    inline void DrawECSInspectorContent(ECS& ecs, uint32_t& selected_entity_id)
    {
        // only construct the draw entries once (static)
        static std::vector<ComponentDrawEntry> draw_entries = {
            MakeDrawEntry<C_Transform>(ecs),
            MakeDrawEntry<C_StaticMesh>(ecs),
            MakeDrawEntry<C_RigidBody>(ecs),
        };

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float total_w = ImGui::GetContentRegionAvail().x;
        const float pane_w = (total_w - spacing) * 0.5f;

        // Left: Entity list
        ImGui::BeginChild("##entity_hierarchy",
            ImVec2(pane_w, 0),
            ImGuiChildFlags_Borders
        );
        DrawEntityList(ecs, selected_entity_id);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right: Component panel
        ImGui::BeginChild("##component_panel",
            ImVec2(pane_w, 0),
            ImGuiChildFlags_Borders
        );
        ImGui::TextDisabled("Inspector");
        ImGui::Separator();
        DrawComponentMaskPanel(ecs, selected_entity_id, draw_entries);
        ImGui::EndChild();
    }
}