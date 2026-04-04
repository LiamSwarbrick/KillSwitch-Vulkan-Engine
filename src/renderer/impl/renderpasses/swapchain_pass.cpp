#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void SwapchainPass_Execute(VkCommandBuffer cmd, RenderPassDesc* desc)
{
    const uint32_t shader_id = SHADER_BLIT;
    PipelineKey key = {
        .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
        .shader_id      = shader_id,
        .pass_type      = desc->pass_type,

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
    ExecuteFullscreenPass(cmd, shader_id, key, push_pass);

    // Draw Debug GUI
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

#if 0 // OLD
    UpdateGlobalSceneData({});  // TODO: Set default camera in renderstate somewhere at some point, instead of it being inside UpdateGlobalSceneData
                                // E.g. for shadow passes, this function will be set from the lights perspective.
                                // It can be updated multiple times within one pass.

    SceneData scene_data = {};
    scene_data.view = renderstate.camera_view;
    scene_data.proj = renderstate.fullscreen_proj;
    scene_data.view_proj = scene_data.proj * scene_data.view;
    UpdateGlobalSceneData(scene_data);

    // NOTE: No specific pass header for unlit forward pass, because it reads no texture.
    // But when adding deferred lighting: it will need the texture and sampler indices to use
    // for sampling the g-buffers. (Similarly for postprocess passes).
    PushConstant_PassHeader push_pass = {};

    // Draw unlit (NOTE: Make sure this is specifically opaques, transparents require a different pipeline key)
    uint32_t shader_id = SHADER_UNLIT;
    uint32_t pass_type = PASS_TYPE_SWAPCHAIN_PASS;
    for (uint32_t i = 0; i < renderstate.drawcalls_collection.array[shader_id].drawcall_count; ++i)
    {
        DrawCall drawcall = renderstate.drawcalls_collection.array[shader_id].drawcalls[i];
        
        PipelineKey key = {
            .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
            .shader_id      = shader_id,
            .pass_type      = pass_type,

            .vertex_type    = drawcall.renderable->mesh_prefab.vertex_type,
            .depth_test     = 1,
            .depth_write    = 0,  // Disable writing
            .depth_op       = VK_COMPARE_OP_EQUAL,  // <- Only draw if it matches prepass exactly (TODO: Is that always a good idea)
            .stencil_mode   = 0,
            .cull_mode      = VK_CULL_MODE_BACK_BIT,
            .blend_mode     = BLEND_MODE_OPAQUE,
            .polygon_mode   = VK_POLYGON_MODE_FILL,  // <- NOTE: When doing debug draw, can use lines
            .front_face     = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .msaa_samples   = PKEY_MULTISAMPLING_1X  // <- Swapchain pass doesn't use msaa
        };

        ExecuteDrawCall(cmd, drawcall, key, push_pass);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
#endif
