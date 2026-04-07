#pragma once

#include "imgui.h"
#include "core/assetsys.h"

namespace DebugUI
{

struct AssetBrowser
{
    enum class Section { None, Mesh, Material, Texture, Image, Node, Animation, Skin, Light };

    Section selected_section = Section::None;
    int     selected_index   = -1;

    void Draw(Asset* asset)
    {
        if (!asset)
        {
            ImGui::TextDisabled("No asset loaded.");
            return;
        }

        // Two-column layout: left = category tree, right = detail panel
        float left_width = 220.0f;
        ImGui::BeginChild("##ab_left", ImVec2(left_width, 0), true);
        DrawCategoryTree(asset);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ab_right", ImVec2(0, 0), true);
        DrawDetail(asset);
        ImGui::EndChild();
    }

private:
    // ---------------------------------------------------------------
    // Left panel
    // ---------------------------------------------------------------
    void DrawCategoryTree(Asset* asset)
    {
        DrawSection(asset, Section::Mesh,      "Meshes",     asset->mesh_count);
        DrawSection(asset, Section::Material,  "Materials",  asset->material_count);
        DrawSection(asset, Section::Texture,   "Textures",   asset->texture_count);
        DrawSection(asset, Section::Image,     "Images",     asset->image_count);
        DrawSection(asset, Section::Node,      "Nodes",      asset->node_count);
        DrawSection(asset, Section::Animation, "Animations", asset->animation_count);
        DrawSection(asset, Section::Skin,      "Skins",      asset->skin_count);
        DrawSection(asset, Section::Light,     "Lights",     asset->light_count);
    }

    void DrawSection(Asset* asset, Section section, const char* label, size_t count)
    {
        if (count == 0) return;

        char header[80];
        snprintf(header, sizeof(header), "%s (%zu)", label, count);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
        if (selected_section == section) flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(header, flags);
        if (ImGui::IsItemClicked())
        {
            selected_section = section;
            selected_index   = -1;
        }

        if (open)
        {
            for (int i = 0; i < (int)count; ++i)
            {
                const char* name = GetItemName(asset, section, i);
                char entry[128];
                if (name && name[0])
                    snprintf(entry, sizeof(entry), "%s", name);
                else
                    snprintf(entry, sizeof(entry), "[%d]", i);

                bool is_selected = (selected_section == section && selected_index == i);
                if (ImGui::Selectable(entry, is_selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    selected_section = section;
                    selected_index   = i;
                }
            }
            ImGui::TreePop();
        }
    }

    const char* GetItemName(Asset* asset, Section section, int i)
    {
        switch (section)
        {
            case Section::Mesh:      return asset->meshes[i].name;
            case Section::Material:  return asset->materials[i].name;
            case Section::Texture:   return asset->textures[i].name;
            case Section::Image:     return asset->images[i].name;
            case Section::Node:      return asset->nodes[i].name;
            case Section::Animation: return asset->animations[i].name;
            case Section::Skin:      return asset->skins[i].name;
            case Section::Light:     return asset->lights[i].name;
            default:                 return nullptr;
        }
    }

    // ---------------------------------------------------------------
    // Right panel
    // ---------------------------------------------------------------
    void DrawDetail(Asset* asset)
    {
        if (selected_section == Section::None || selected_index < 0)
        {
            ImGui::TextDisabled("Select an item on the left.");
            return;
        }

        switch (selected_section)
        {
            case Section::Mesh:      DrawMeshDetail(asset);      break;
            case Section::Material:  DrawMaterialDetail(asset);  break;
            case Section::Texture:   DrawTextureDetail(asset);   break;
            case Section::Image:     DrawImageDetail(asset);     break;
            case Section::Node:      DrawNodeDetail(asset);      break;
            case Section::Animation: DrawAnimationDetail(asset); break;
            case Section::Skin:      DrawSkinDetail(asset);      break;
            case Section::Light:     DrawLightDetail(asset);     break;
            default: break;
        }
    }

    void DrawMeshDetail(Asset* asset)
    {
        Mesh& m = asset->meshes[selected_index];
        ImGui::Text("Name:        %s", m.name ? m.name : "(unnamed)");
        ImGui::Text("Vertex Type: %s", m.vertex_type == 0 ? "Static" : "Animated");
        ImGui::Text("Primitives:  %zu", m.primitive_count);
        ImGui::Separator();

        for (size_t i = 0; i < m.primitive_count; ++i)
        {
            Primitive& p = m.primitives[i];
            if (ImGui::TreeNodeEx((void*)(intptr_t)i, ImGuiTreeNodeFlags_DefaultOpen,
                                  "Primitive [%zu]", i))
            {
                ImGui::Text("Vertices:  %zu", p.vertex_count);
                ImGui::Text("Indices:   %zu", p.index_count);
                ImGui::Text("Material:  %d", p.material_index);
                ImGui::Text("Has normals:  %s", p.normals   ? "yes" : "no");
                ImGui::Text("Has texcoords:%s", p.texcoords ? "yes" : "no");
                ImGui::Text("Has skinning: %s", p.joints    ? "yes" : "no");
                ImGui::TreePop();
            }
        }
    }

    void DrawMaterialDetail(Asset* asset)
    {
        Material& mat = asset->materials[selected_index];
        ImGui::Text("Name: %s", mat.name ? mat.name : "(unnamed)");
        ImGui::Separator();
        ImGui::ColorEdit4("Base Color", mat.base_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
        ImGui::Text("Metallic:      %.3f", mat.metallic);
        ImGui::Text("Roughness:     %.3f", mat.roughness);
        ImGui::Text("Alpha Cutoff:  %.3f", mat.alpha_cutoff);
        ImGui::Text("Emissive:      %.2f  %.2f  %.2f",
                    mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]);
        ImGui::Separator();
        ImGui::Text("Texture indices:");
        ImGui::Text("  Base Color:         %d", mat.base_color_texture_index);
        ImGui::Text("  Metallic/Roughness: %d", mat.metallic_roughness_texture_index);
        ImGui::Text("  Normal Map:         %d", mat.normal_map_texture_index);
        ImGui::Text("  Emissive:           %d", mat.emissive_texture_index);
        ImGui::Text("  Occlusion:          %d", mat.occlusion_texture_index);
    }

    void DrawTextureDetail(Asset* asset)
    {
        Texture& t = asset->textures[selected_index];
        ImGui::Text("Name:       %s", t.name ? t.name : "(unnamed)");
        ImGui::Text("Image idx:  %d", t.image_index);
        const char* mag[] = { "NEAREST", "LINEAR" };
        const char* wrap[] = { "CLAMP_TO_EDGE", "MIRRORED_REPEAT", "REPEAT" };
        // cgltf filter values: 9728=NEAREST, 9729=LINEAR etc.
        ImGui::Text("Mag filter: %d", t.mag_filter);
        ImGui::Text("Min filter: %d", t.min_filter);
        ImGui::Text("Wrap S:     %d", t.wrap_s);
        ImGui::Text("Wrap T:     %d", t.wrap_t);
    }

    void DrawImageDetail(Asset* asset)
    {
        Image& img = asset->images[selected_index];
        ImGui::Text("Name:  %s", img.name ? img.name : "(unnamed)");
        ImGui::Text("URI:   %s", img.uri  ? img.uri  : "(embedded)");
        ImGui::Text("Size:  %u x %u", img.width, img.height);
        ImGui::Text("Bytes: %zu", img.data_size);
    }

    void DrawNodeDetail(Asset* asset)
    {
        Node& n = asset->nodes[selected_index];
        ImGui::Text("Name:   %s", n.name ? n.name : "(unnamed)");
        ImGui::Text("Parent: %d", n.parent_index);
        ImGui::Text("Children: %zu", n.child_count);
        ImGui::Separator();
        ImGui::Text("Translation: %.3f  %.3f  %.3f",
                    n.translation[0], n.translation[1], n.translation[2]);
        ImGui::Text("Rotation:    %.3f  %.3f  %.3f  %.3f",
                    n.rotation[0], n.rotation[1], n.rotation[2], n.rotation[3]);
        ImGui::Text("Scale:       %.3f  %.3f  %.3f",
                    n.scale[0], n.scale[1], n.scale[2]);
        ImGui::Separator();
        ImGui::Text("Mesh:   %d", n.mesh_index);
        ImGui::Text("Camera: %d", n.camera_index);
        ImGui::Text("Light:  %d", n.light_index);
        ImGui::Text("Skin:   %d", n.skin_index);
        if (n.extras_json)
        {
            ImGui::Separator();
            ImGui::TextWrapped("Extras JSON:\n%s", n.extras_json);
        }
    }

    void DrawAnimationDetail(Asset* asset)
    {
        Animation& anim = asset->animations[selected_index];
        ImGui::Text("Name:     %s", anim.name ? anim.name : "(unnamed)");
        ImGui::Text("Samplers: %zu", anim.sampler_count);
        ImGui::Text("Channels: %zu", anim.channel_count);
        ImGui::Separator();
        const char* paths[] = { "translation", "rotation", "scale", "weights" };
        const char* interps[] = { "LINEAR", "STEP", "CUBICSPLINE" };
        for (size_t i = 0; i < anim.channel_count; ++i)
        {
            AnimationChannel& ch = anim.channels[i];
            const char* path  = (ch.target_path  >= 0 && ch.target_path  < 4) ? paths[ch.target_path]   : "?";
            const char* node_name = (ch.target_node_index >= 0 && ch.target_node_index < (int)asset->node_count)
                                   ? asset->nodes[ch.target_node_index].name : "?";
            ImGui::Text("  [%zu] node=%s  path=%s", i, node_name ? node_name : "?", path);
        }
    }

    void DrawSkinDetail(Asset* asset)
    {
        Skin& s = asset->skins[selected_index];
        ImGui::Text("Name:          %s", s.name ? s.name : "(unnamed)");
        ImGui::Text("Joints:        %zu", s.joint_count);
        ImGui::Text("Skeleton root: %d", s.skeleton_root_node_index);
        ImGui::Separator();
        for (size_t i = 0; i < s.joint_count && i < 64; ++i)
        {
            Bone& b = s.bones[i];
            ImGui::Text("  [%zu] %s  parent=%d  children=%zu",
                        i, b.name ? b.name : "?", b.parent_index, b.child_count);
        }
        if (s.joint_count > 64)
            ImGui::TextDisabled("  ... (%zu more)", s.joint_count - 64);
    }

    void DrawLightDetail(Asset* asset)
    {
        Light& l = asset->lights[selected_index];
        const char* types[] = { "Directional", "Point", "Spot" };
        ImGui::Text("Name:      %s", l.name ? l.name : "(unnamed)");
        ImGui::Text("Type:      %s", (l.type >= 0 && l.type < 3) ? types[l.type] : "?");
        ImGui::ColorEdit3("Color", l.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
        ImGui::Text("Intensity: %.3f", l.intensity);
        ImGui::Text("Range:     %.3f", l.range);
    }
};

}  // namespace DebugUI
