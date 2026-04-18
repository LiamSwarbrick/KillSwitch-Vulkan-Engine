#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void SwapchainPass_Execute(VkCommandBuffer cmd, uint32_t pass_idx)
{
    RenderPassDesc* desc = &renderstate.framegraph.passes[pass_idx];

    uint64_t scene_ptr = 0;
    {
        SceneData scene_data = {};

        // Unused matrices
        glm::mat4 view, proj, view_proj;
        view = glm::mat4(1.0f);
        proj = glm::mat4(1.0f);
        view_proj = proj * view;

        VkExtent2D extents = renderstate.swapchain_extent;
        glm::uvec2 extents_uvec2 = glm::uvec2(extents.width, extents.height);

        memcpy(scene_data.view, glm::value_ptr(view), sizeof(glm::mat4));
        memcpy(scene_data.proj, glm::value_ptr(proj), sizeof(glm::mat4));
        memcpy(scene_data.view_proj, glm::value_ptr(view_proj), sizeof(glm::mat4));
        memcpy(scene_data.rendertarget_size, glm::value_ptr(extents_uvec2), sizeof(glm::uvec2));

        scene_data.aspect = (float)extents.width / (float)extents.height;
        scene_ptr = PushToMappedArena(&renderstate.scenes_arena, &scene_data, sizeof(SceneData));
    }

    const uint32_t shader_id = SHADER_BLIT;
    PipelineKey key = {
        .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
        .shader_id      = shader_id,
        .pass_idx       = pass_idx,

        // Ignore this shit
        .vertex_type    = 0,
        .depth_test     = 0,
        .depth_write    = 0,
        .depth_op       = 0,
        .stencil_mode   = 0,

        .cull_mode      = VK_CULL_MODE_BACK_BIT,
        .blend_mode     = BLEND_MODE_OPAQUE,
        .polygon_mode   = VK_POLYGON_MODE_FILL,
        .front_face     = VK_FRONT_FACE_CLOCKWISE,  // <- Fullscreen tri in the shader is clockwise
        .msaa_samples   = PKEY_MULTISAMPLING_1X     // <- Swapchain pass doesn't use msaa
    };

    PushConstant_PassHeader push_pass = {};
    push_pass.texture_indices[0] = renderstate.registry.resources[renderstate.rids.hdr_color_target_rid].image_bindless_index;
    ExecuteFullscreenPass(cmd, shader_id, key, push_pass, scene_ptr);

    // Draw Debug GUI
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
