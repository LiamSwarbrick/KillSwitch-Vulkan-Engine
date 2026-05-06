#pragma once

#include "imgui.h"
#include "core/assetsys.h"
#include "renderer/renderer.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <vector>

namespace DebugUI
{

struct AssetBrowser
{
    static constexpr float k_thumbnail_size = 100.0f;

    struct TexturePreview
    {
        Renderer_UITexture texture = {};
        bool attempted = false;
        bool loaded = false;
    };

    int selected_asset_index = -1;

    int selected_mesh      = -1;
    int selected_material  = -1;
    int selected_texture   = -1;
    int selected_image     = -1;
    int selected_node      = -1;
    int selected_animation = -1;
    int selected_skin      = -1;
    int selected_light     = -1;

    std::unordered_map<const Image*, TexturePreview> texture_preview_cache;

    void Draw(std::vector<Asset*>* assets, Asset*& selected_asset)
    {
        if (assets == nullptr || assets->empty())
        {
            selected_asset = nullptr;
            selected_asset_index = -1;
            ImGui::TextDisabled("No loaded assets.");
            return;
        }

        SyncSelection(*assets, selected_asset);

        const float left_width = k_thumbnail_size + 52.0f;
        ImGui::BeginChild("##ab_left", ImVec2(left_width, 0.0f), true);
        DrawAssetThumbnailGrid(*assets, selected_asset);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ab_right", ImVec2(0.0f, 0.0f), true);
        DrawRightPanel(selected_asset);
        ImGui::EndChild();
    }

private:
    static const char* SafeName(const char* name)
    {
        return (name && name[0]) ? name : "(unnamed)";
    }

    static const char* ExtractFileName(const char* path)
    {
        if (!path || !path[0])
            return nullptr;

        const char* slash = strrchr(path, '/');
        const char* backslash = strrchr(path, '\\');

        if (slash && backslash)
            return (slash > backslash) ? (slash + 1) : (backslash + 1);
        if (slash)
            return slash + 1;
        if (backslash)
            return backslash + 1;
        return path;
    }

    void BuildModelName(Asset* asset, char* out_name, size_t out_size) const
    {
        if (asset && asset->node_count > 0 && asset->nodes[0].name && asset->nodes[0].name[0])
        {
            snprintf(out_name, out_size, "%s", asset->nodes[0].name);
            return;
        }

        if (asset && asset->mesh_count > 0 && asset->meshes[0].name && asset->meshes[0].name[0])
        {
            snprintf(out_name, out_size, "%s", asset->meshes[0].name);
            return;
        }

        snprintf(out_name, out_size, "(unnamed model)");
    }

    void BuildAssetName(Asset* asset, int fallback_index, char* out_name, size_t out_size) const
    {
        const char* file_name = (asset != nullptr) ? ExtractFileName(asset->source_path) : nullptr;
        if (file_name && file_name[0])
        {
            snprintf(out_name, out_size, "%s", file_name);
            return;
        }

        snprintf(out_name, out_size, "Asset %d", fallback_index);
    }

    static bool LooksLikeFilesystemPath(const char* uri)
    {
        if (!uri || !uri[0])
            return false;

        return (strchr(uri, '/') != nullptr) || (strchr(uri, '\\') != nullptr);
    }

    TexturePreview& EnsureTexturePreview(const Image* image)
    {
        TexturePreview& cached = texture_preview_cache[image];
        if (cached.attempted)
            return cached;

        cached.attempted = true;

        if (!image || !image->uri || !image->uri[0])
            return cached;

        // External files in this project normally carry a path; embedded textures usually don't.
        if (!LooksLikeFilesystemPath(image->uri))
            return cached;

        Renderer_UITexture texture = {};
        if (Renderer_LoadUITexture(image->uri, false, &texture))
        {
            cached.texture = texture;
            cached.loaded = true;
        }

        return cached;
    }

    void DrawTexturePreview(const Image* image)
    {
        TexturePreview& preview = EnsureTexturePreview(image);
        if (!preview.loaded || preview.texture.imgui_texture_id == 0)
        {
            ImGui::TextDisabled("Preview unavailable (embedded texture or load failed).");
            return;
        }

        float draw_w = ImGui::GetContentRegionAvail().x;
        if (draw_w < 64.0f) draw_w = 64.0f;
        if (draw_w > 300.0f) draw_w = 300.0f;

        float aspect = 1.0f;
        if (image && image->width > 0 && image->height > 0)
            aspect = (float)image->height / (float)image->width;
        else if (preview.texture.width > 0 && preview.texture.height > 0)
            aspect = (float)preview.texture.height / (float)preview.texture.width;

        float draw_h = draw_w * aspect;
        if (draw_h > 220.0f)
        {
            draw_h = 220.0f;
            draw_w = draw_h / aspect;
        }

        ImGui::Image((ImTextureID)preview.texture.imgui_texture_id, ImVec2(draw_w, draw_h));
    }

    void ResetDetailSelectionFor(Asset* asset)
    {
        selected_mesh      = (asset && asset->mesh_count > 0) ? 0 : -1;
        selected_material  = (asset && asset->material_count > 0) ? 0 : -1;
        selected_texture   = (asset && asset->texture_count > 0) ? 0 : -1;
        selected_image     = (asset && asset->image_count > 0) ? 0 : -1;
        selected_node      = (asset && asset->node_count > 0) ? 0 : -1;
        selected_animation = (asset && asset->animation_count > 0) ? 0 : -1;
        selected_skin      = (asset && asset->skin_count > 0) ? 0 : -1;
        selected_light     = (asset && asset->light_count > 0) ? 0 : -1;
    }

    void SyncSelection(std::vector<Asset*>& assets, Asset*& selected_asset)
    {
        int old_index = selected_asset_index;

        if (selected_asset)
        {
            auto it = std::find(assets.begin(), assets.end(), selected_asset);
            if (it != assets.end())
                selected_asset_index = (int)std::distance(assets.begin(), it);
        }

        if (selected_asset_index < 0 || selected_asset_index >= (int)assets.size())
            selected_asset_index = 0;

        selected_asset = assets[selected_asset_index];

        if (old_index != selected_asset_index)
            ResetDetailSelectionFor(selected_asset);
        else if (selected_mesh < -1)
            ResetDetailSelectionFor(selected_asset);
    }

    static bool ProjectPoint(
        const float* pos,
        float cx,
        float cy,
        float cz,
        float inv_scale,
        float yaw,
        float pitch,
        float half_w,
        float half_h,
        float center_x,
        float center_y,
        ImVec2& out
    )
    {
        float x = (pos[0] - cx) * inv_scale;
        float y = (pos[1] - cy) * inv_scale;
        float z = (pos[2] - cz) * inv_scale;

        const float cyaw = cosf(yaw);
        const float syaw = sinf(yaw);
        const float cpitch = cosf(pitch);
        const float spitch = sinf(pitch);

        float xr = cyaw * x + syaw * z;
        float zr = -syaw * x + cyaw * z;

        float yr = cpitch * y - spitch * zr;
        float zr2 = spitch * y + cpitch * zr;

        const float camera_distance = 3.0f;
        float view_z = zr2 + camera_distance;
        if (view_z <= 0.1f)
            return false;

        const float scale = (half_w < half_h ? half_w : half_h) * 1.3f;
        out.x = center_x + (xr / view_z) * scale;
        out.y = center_y - (yr / view_z) * scale;
        return true;
    }

    void DrawWireframeThumbnail(Asset* asset, const ImVec2& min_p, const ImVec2& max_p, bool selected)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImU32 bg_col = selected ? IM_COL32(42, 79, 128, 255) : IM_COL32(24, 26, 31, 255);
        ImU32 border_col = selected ? IM_COL32(100, 170, 255, 255) : IM_COL32(70, 74, 82, 255);
        ImU32 line_col = selected ? IM_COL32(190, 230, 255, 255) : IM_COL32(200, 210, 230, 220);

        draw_list->AddRectFilled(min_p, max_p, bg_col, 5.0f);
        draw_list->AddRect(min_p, max_p, border_col, 5.0f, 0, selected ? 2.0f : 1.0f);

        if (!asset || asset->mesh_count == 0)
        {
            draw_list->AddText(ImVec2(min_p.x + 8.0f, min_p.y + 8.0f), IM_COL32(170, 170, 170, 255), "No mesh");
            return;
        }

        struct PrimitiveInfo
        {
            Primitive* prim;
            glm::mat4 world;
            size_t triangle_count;
        };

        std::vector<PrimitiveInfo> primitive_infos;
        primitive_infos.reserve(32);

        float min_x = FLT_MAX, min_y = FLT_MAX, min_z = FLT_MAX;
        float max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;
        size_t total_triangles = 0;

        auto is_non_identity_matrix = [](const float* m) -> bool
        {
            const float ident[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };

            for (int i = 0; i < 16; ++i)
            {
                if (fabsf(m[i] - ident[i]) > 1e-6f)
                    return true;
            }

            return false;
        };

        auto node_local_matrix = [&](const Node& node) -> glm::mat4
        {
            if (is_non_identity_matrix(node.matrix))
                return glm::make_mat4(node.matrix);

            glm::vec3 t = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);

            glm::quat q = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            if (glm::dot(q, q) < 1e-8f)
                q = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            else
                q = glm::normalize(q);

            glm::vec3 s = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            if (fabsf(s.x) < 1e-8f && fabsf(s.y) < 1e-8f && fabsf(s.z) < 1e-8f)
                s = glm::vec3(1.0f, 1.0f, 1.0f);

            return glm::translate(glm::mat4(1.0f), t)
                * glm::mat4_cast(q)
                * glm::scale(glm::mat4(1.0f), s);
        };

        auto accumulate_primitive = [&](Primitive& p, const glm::mat4& world)
        {
            if (!p.positions || p.vertex_count == 0)
                return;

            for (size_t vi = 0; vi < p.vertex_count; ++vi)
            {
                const float* v = &p.positions[vi * 3];
                glm::vec4 w = world * glm::vec4(v[0], v[1], v[2], 1.0f);
                if (w.x < min_x) min_x = w.x;
                if (w.y < min_y) min_y = w.y;
                if (w.z < min_z) min_z = w.z;
                if (w.x > max_x) max_x = w.x;
                if (w.y > max_y) max_y = w.y;
                if (w.z > max_z) max_z = w.z;
            }

            size_t tri_count = 0;
            if (p.indices && p.index_count >= 3)
                tri_count = p.index_count / 3;
            else
                tri_count = p.vertex_count / 3;

            if (tri_count == 0)
                return;

            primitive_infos.push_back({ &p, world, tri_count });
            total_triangles += tri_count;
        };

        bool has_node_mesh_instances = false;

        if (asset->node_count > 0)
        {
            const int node_count = (int)asset->node_count;
            std::vector<glm::mat4> world_mats(asset->node_count, glm::mat4(1.0f));
            std::vector<uint8_t> visit_state(asset->node_count, 0);  // 0=unvisited, 1=visiting, 2=done

            std::function<glm::mat4(int)> compute_world = [&](int idx) -> glm::mat4
            {
                if (idx < 0 || idx >= node_count)
                    return glm::mat4(1.0f);

                if (visit_state[idx] == 2)
                    return world_mats[idx];

                if (visit_state[idx] == 1)
                    return glm::mat4(1.0f);  // Cycle guard.

                visit_state[idx] = 1;

                const Node& node = asset->nodes[idx];
                glm::mat4 local = node_local_matrix(node);
                int parent = node.parent_index;
                if (parent >= 0 && parent < node_count)
                    world_mats[idx] = compute_world(parent) * local;
                else
                    world_mats[idx] = local;

                visit_state[idx] = 2;
                return world_mats[idx];
            };

            for (int ni = 0; ni < node_count; ++ni)
            {
                const Node& node = asset->nodes[ni];
                if (node.mesh_index < 0 || node.mesh_index >= (int)asset->mesh_count)
                    continue;

                has_node_mesh_instances = true;

                glm::mat4 world = compute_world(ni);
                Mesh& mesh = asset->meshes[node.mesh_index];
                for (size_t pi = 0; pi < mesh.primitive_count; ++pi)
                    accumulate_primitive(mesh.primitives[pi], world);
            }
        }

        if (!has_node_mesh_instances)
        {
            const glm::mat4 identity = glm::mat4(1.0f);
            for (size_t mi = 0; mi < asset->mesh_count; ++mi)
            {
                Mesh& mesh = asset->meshes[mi];
                for (size_t pi = 0; pi < mesh.primitive_count; ++pi)
                    accumulate_primitive(mesh.primitives[pi], identity);
            }
        }

        if (primitive_infos.empty())
        {
            draw_list->AddText(ImVec2(min_p.x + 8.0f, min_p.y + 8.0f), IM_COL32(170, 170, 170, 255), "No verts");
            return;
        }

        float cx = (min_x + max_x) * 0.5f;
        float cy = (min_y + max_y) * 0.5f;
        float cz = (min_z + max_z) * 0.5f;

        float extent_x = max_x - min_x;
        float extent_y = max_y - min_y;
        float extent_z = max_z - min_z;
        float extent = extent_x;
        if (extent_y > extent) extent = extent_y;
        if (extent_z > extent) extent = extent_z;
        if (extent < 0.0001f) extent = 1.0f;

        float inv_scale = 1.0f / extent;

        const float margin = 8.0f;
        float x0 = min_p.x + margin;
        float y0 = min_p.y + margin;
        float x1 = max_p.x - margin;
        float y1 = max_p.y - margin;

        float half_w = (x1 - x0) * 0.5f;
        float half_h = (y1 - y0) * 0.5f;
        float center_x = x0 + half_w;
        float center_y = y0 + half_h;

        const float yaw = 0.85f;
        const float pitch = -0.45f;

        auto draw_tri = [&](const PrimitiveInfo& info, uint32_t ia, uint32_t ib, uint32_t ic)
        {
            Primitive* p = info.prim;
            if (ia >= p->vertex_count || ib >= p->vertex_count || ic >= p->vertex_count)
                return;

            const float* a = &p->positions[ia * 3];
            const float* b = &p->positions[ib * 3];
            const float* c = &p->positions[ic * 3];

            glm::vec4 wa = info.world * glm::vec4(a[0], a[1], a[2], 1.0f);
            glm::vec4 wb = info.world * glm::vec4(b[0], b[1], b[2], 1.0f);
            glm::vec4 wc = info.world * glm::vec4(c[0], c[1], c[2], 1.0f);

            float pa3[3] = { wa.x, wa.y, wa.z };
            float pb3[3] = { wb.x, wb.y, wb.z };
            float pc3[3] = { wc.x, wc.y, wc.z };

            ImVec2 pa, pb, pc;
            bool oka = ProjectPoint(pa3, cx, cy, cz, inv_scale, yaw, pitch, half_w, half_h, center_x, center_y, pa);
            bool okb = ProjectPoint(pb3, cx, cy, cz, inv_scale, yaw, pitch, half_w, half_h, center_x, center_y, pb);
            bool okc = ProjectPoint(pc3, cx, cy, cz, inv_scale, yaw, pitch, half_w, half_h, center_x, center_y, pc);
            if (!(oka && okb && okc))
                return;

            draw_list->AddLine(pa, pb, line_col, 1.0f);
            draw_list->AddLine(pb, pc, line_col, 1.0f);
            draw_list->AddLine(pc, pa, line_col, 1.0f);
        };

        const size_t tri_budget_total = 2400;
        size_t remaining_budget = tri_budget_total;

        for (size_t i = 0; i < primitive_infos.size(); ++i)
        {
            if (remaining_budget == 0)
                break;

            const PrimitiveInfo& info = primitive_infos[i];
            Primitive* p = info.prim;
            size_t tri_total = info.triangle_count;
            if (tri_total == 0)
                continue;

            size_t draw_count = tri_total;
            if (total_triangles > tri_budget_total)
            {
                draw_count = (tri_total * tri_budget_total) / total_triangles;
                if (draw_count == 0)
                    draw_count = 1;
            }

            if (draw_count > tri_total)
                draw_count = tri_total;
            if (draw_count > remaining_budget)
                draw_count = remaining_budget;
            if (draw_count == 0)
                continue;

            size_t step = tri_total / draw_count;
            if (step < 1)
                step = 1;

            size_t drawn = 0;
            if (p->indices && p->index_count >= 3)
            {
                for (size_t t = 0; t < tri_total && drawn < draw_count; t += step)
                {
                    uint32_t ia = p->indices[t * 3 + 0];
                    uint32_t ib = p->indices[t * 3 + 1];
                    uint32_t ic = p->indices[t * 3 + 2];
                    draw_tri(info, ia, ib, ic);
                    ++drawn;
                }
            }
            else
            {
                for (size_t t = 0; t < tri_total && drawn < draw_count; t += step)
                {
                    uint32_t ia = (uint32_t)(t * 3 + 0);
                    uint32_t ib = (uint32_t)(t * 3 + 1);
                    uint32_t ic = (uint32_t)(t * 3 + 2);
                    draw_tri(info, ia, ib, ic);
                    ++drawn;
                }
            }

            if (drawn > remaining_budget)
                remaining_budget = 0;
            else
                remaining_budget -= drawn;
        }
    }

    void DrawAssetThumbnailGrid(std::vector<Asset*>& assets, Asset*& selected_asset)
    {
        ImGui::Text("Loaded Models: %d", (int)assets.size());
        ImGui::Separator();

        const float tile_size = k_thumbnail_size;

        for (int i = 0; i < (int)assets.size(); ++i)
        {
            Asset* asset = assets[i];
            bool is_selected = (i == selected_asset_index);

            ImGui::PushID(i);

            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##asset_thumb", ImVec2(tile_size, tile_size));
            ImVec2 p1 = ImGui::GetItemRectMax();

            DrawWireframeThumbnail(asset, p0, p1, is_selected);

            if (ImGui::IsItemClicked())
            {
                selected_asset_index = i;
                selected_asset = asset;
                ResetDetailSelectionFor(selected_asset);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                char model_name[128];
                BuildModelName(asset, model_name, sizeof(model_name));
                ImGui::Text("Model: %s", model_name);
                ImGui::Text("File:  %s", (asset && asset->source_path) ? asset->source_path : "(unknown)");
                ImGui::EndTooltip();
            }

            char name[128];
            BuildAssetName(asset, i, name, sizeof(name));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tile_size);
            ImGui::TextUnformatted(name);
            ImGui::PopTextWrapPos();

            ImGui::PopID();

            if ((i + 1) < (int)assets.size())
                ImGui::Spacing();
        }
    }

    template <typename NameGetter>
    bool DrawIndexedCombo(const char* label, size_t count, int& selected, NameGetter get_name)
    {
        if (count == 0)
        {
            selected = -1;
            ImGui::TextDisabled("No entries.");
            return false;
        }

        if (selected < 0 || selected >= (int)count)
            selected = 0;

        char preview[192];
        snprintf(preview, sizeof(preview), "[%d] %s", selected, SafeName(get_name(selected)));

        if (ImGui::BeginCombo(label, preview))
        {
            for (int i = 0; i < (int)count; ++i)
            {
                char entry[192];
                snprintf(entry, sizeof(entry), "[%d] %s", i, SafeName(get_name(i)));

                bool is_selected = (i == selected);
                if (ImGui::Selectable(entry, is_selected))
                    selected = i;

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return true;
    }

    void DrawRightPanel(Asset* asset)
    {
        if (!asset)
        {
            ImGui::TextDisabled("No asset selected.");
            return;
        }

        char asset_name[128];
        BuildAssetName(asset, selected_asset_index, asset_name, sizeof(asset_name));
        ImGui::Text("Selected: %s", asset_name);
        ImGui::Separator();

        ImGui::BulletText("Meshes: %zu", asset->mesh_count);
        ImGui::BulletText("Materials: %zu", asset->material_count);
        ImGui::BulletText("Textures: %zu", asset->texture_count);
        ImGui::BulletText("Images: %zu", asset->image_count);
        ImGui::BulletText("Nodes: %zu", asset->node_count);
        ImGui::BulletText("Animations: %zu", asset->animation_count);
        ImGui::BulletText("Skins: %zu", asset->skin_count);
        ImGui::BulletText("Lights: %zu", asset->light_count);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawIndexedCombo("Mesh##ab", asset->mesh_count, selected_mesh, [&](int i) { return asset->meshes[i].name; }))
            {
                ImGui::Separator();
                DrawMeshDetail(asset, selected_mesh);
            }
        }

        if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawIndexedCombo("Material##ab", asset->material_count, selected_material, [&](int i) { return asset->materials[i].name; }))
            {
                ImGui::Separator();
                DrawMaterialDetail(asset, selected_material);
            }
        }

        if (ImGui::CollapsingHeader("Textures"))
        {
            if (DrawIndexedCombo("Texture##ab", asset->texture_count, selected_texture, [&](int i) { return asset->textures[i].name; }))
            {
                ImGui::Separator();
                DrawTextureDetail(asset, selected_texture);
            }
        }

        if (ImGui::CollapsingHeader("Images"))
        {
            if (DrawIndexedCombo("Image##ab", asset->image_count, selected_image, [&](int i) { return asset->images[i].name; }))
            {
                ImGui::Separator();
                DrawImageDetail(asset, selected_image);
            }
        }

        if (ImGui::CollapsingHeader("Nodes"))
        {
            if (DrawIndexedCombo("Node##ab", asset->node_count, selected_node, [&](int i) { return asset->nodes[i].name; }))
            {
                ImGui::Separator();
                DrawNodeDetail(asset, selected_node);
            }
        }

        if (ImGui::CollapsingHeader("Animations"))
        {
            if (DrawIndexedCombo("Animation##ab", asset->animation_count, selected_animation, [&](int i) { return asset->animations[i].name; }))
            {
                ImGui::Separator();
                DrawAnimationDetail(asset, selected_animation);
            }
        }

        if (ImGui::CollapsingHeader("Skins"))
        {
            if (DrawIndexedCombo("Skin##ab", asset->skin_count, selected_skin, [&](int i) { return asset->skins[i].name; }))
            {
                ImGui::Separator();
                DrawSkinDetail(asset, selected_skin);
            }
        }

        if (ImGui::CollapsingHeader("Lights"))
        {
            if (DrawIndexedCombo("Light##ab", asset->light_count, selected_light, [&](int i) { return asset->lights[i].name; }))
            {
                ImGui::Separator();
                DrawLightDetail(asset, selected_light);
            }
        }
    }

    void DrawMeshDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->mesh_count) return;

        Mesh& m = asset->meshes[index];
        ImGui::Text("Name:        %s", SafeName(m.name));
        ImGui::Text("Vertex Type: %s", m.vertex_type == 0 ? "Static" : "Animated");
        ImGui::Text("Primitives:  %zu", m.primitive_count);
        ImGui::Separator();

        for (size_t i = 0; i < m.primitive_count; ++i)
        {
            Primitive& p = m.primitives[i];
            if (ImGui::TreeNodeEx((void*)(intptr_t)i, ImGuiTreeNodeFlags_DefaultOpen, "Primitive [%zu]", i))
            {
                ImGui::Text("Vertices:      %zu", p.vertex_count);
                ImGui::Text("Indices:       %zu", p.index_count);
                ImGui::Text("Material:      %d", p.material_index);
                ImGui::Text("Has normals:   %s", p.normals ? "yes" : "no");
                ImGui::Text("Has texcoords: %s", p.texcoords ? "yes" : "no");
                ImGui::Text("Has skinning:  %s", p.joints ? "yes" : "no");
                ImGui::TreePop();
            }
        }
    }

    void DrawMaterialDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->material_count) return;

        Material& mat = asset->materials[index];
        ImGui::Text("Name: %s", SafeName(mat.name));
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

    void DrawTextureDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->texture_count) return;

        Texture& t = asset->textures[index];
        ImGui::Text("Name:       %s", SafeName(t.name));
        ImGui::Text("Image idx:  %d", t.image_index);
        ImGui::Text("Mag filter: %d", t.mag_filter);
        ImGui::Text("Min filter: %d", t.min_filter);
        ImGui::Text("Wrap S:     %d", t.wrap_s);
        ImGui::Text("Wrap T:     %d", t.wrap_t);

        ImGui::Separator();
        ImGui::Text("Texture Preview:");
        if (t.image_index >= 0 && t.image_index < (int)asset->image_count)
            DrawTexturePreview(&asset->images[t.image_index]);
        else
            ImGui::TextDisabled("No linked image.");
    }

    void DrawImageDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->image_count) return;

        Image& img = asset->images[index];
        ImGui::Text("Name:  %s", SafeName(img.name));
        ImGui::Text("URI:   %s", img.uri ? img.uri : "(embedded)");
        ImGui::Text("Size:  %u x %u", img.width, img.height);
        ImGui::Text("Bytes: %zu", img.data_size);
    }

    void DrawNodeDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->node_count) return;

        Node& n = asset->nodes[index];
        ImGui::Text("Name:      %s", SafeName(n.name));
        ImGui::Text("Parent:    %d", n.parent_index);
        ImGui::Text("Children:  %zu", n.child_count);
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

    void DrawAnimationDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->animation_count) return;

        Animation& anim = asset->animations[index];
        ImGui::Text("Name:     %s", SafeName(anim.name));
        ImGui::Text("Samplers: %zu", anim.sampler_count);
        ImGui::Text("Channels: %zu", anim.channel_count);
        ImGui::Separator();
        const char* paths[] = { "translation", "rotation", "scale", "weights" };
        for (size_t i = 0; i < anim.channel_count; ++i)
        {
            AnimationChannel& ch = anim.channels[i];
            const char* path = (ch.target_path >= 0 && ch.target_path < 4) ? paths[ch.target_path] : "?";
            const char* node_name = (ch.target_node_index >= 0 && ch.target_node_index < (int)asset->node_count)
                ? SafeName(asset->nodes[ch.target_node_index].name) : "?";
            ImGui::Text("  [%zu] node=%s  path=%s", i, node_name, path);
        }
    }

    void DrawSkinDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->skin_count) return;

        Skin& s = asset->skins[index];
        ImGui::Text("Name:          %s", SafeName(s.name));
        ImGui::Text("Joints:        %zu", s.joint_count);
        ImGui::Text("Skeleton root: %d", s.skeleton_root_node_index);
        ImGui::Separator();
        for (size_t i = 0; i < s.joint_count && i < 64; ++i)
        {
            Bone& b = s.bones[i];
            ImGui::Text("  [%zu] %s  parent=%d  children=%zu",
                        i, SafeName(b.name), b.parent_index, b.child_count);
        }
        if (s.joint_count > 64)
            ImGui::TextDisabled("  ... (%zu more)", s.joint_count - 64);
    }

    void DrawLightDetail(Asset* asset, int index)
    {
        if (index < 0 || index >= (int)asset->light_count) return;

        Light& l = asset->lights[index];
        const char* types[] = { "Directional", "Point", "Spot" };
        ImGui::Text("Name:      %s", SafeName(l.name));
        ImGui::Text("Type:      %s", (l.type >= 0 && l.type < 3) ? types[l.type] : "?");
        ImGui::ColorEdit3("Color", l.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
        ImGui::Text("Intensity: %.3f", l.intensity);
        ImGui::Text("Range:     %.3f", l.range);
    }
};

}  // namespace DebugUI
