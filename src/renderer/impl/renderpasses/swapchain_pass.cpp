#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void FullscreenPass_Execute(VkCommandBuffer cmd, uint32_t pass_idx)
{
    RenderPassDesc* desc = &renderstate.framegraph.passes[pass_idx];
    FullscreenPass_UserData* user_data = (FullscreenPass_UserData*)desc->user_data;
    SDL_assert(desc->output_count == 1 && "The fullscreen pass is intended for a single colour only attachment.");

    uint64_t scene_ptr = 0;
    {
        // NOTE: Empty camera info, so don't use it bcuz view matrix is all zeroes
        VkExtent3D extent = renderstate.registry.resources[desc->outputs[0].rid].image.extent;
        SceneData scene_data = MakeSceneData((CameraInfo){}, (VkExtent2D){ extent.width, extent.height });
        scene_ptr = PushToMappedArena(&renderstate.scenes_arena, &scene_data, sizeof(SceneData));
    }

    const uint32_t shader_id = user_data->shader_id;
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
        .msaa_samples   = PKEY_MULTISAMPLING_1X     // <- Postprocess pass doesn't use msaa
    };

    ExecuteFullscreenPass(cmd, shader_id, key, user_data->push_pass, scene_ptr);

    // Draw Debug GUI
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
