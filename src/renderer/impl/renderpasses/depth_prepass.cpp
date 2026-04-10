#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void DepthPrepass_Execute(VkCommandBuffer cmd, RenderPassDesc* desc)
{
    const uint32_t pass_type = PASS_TYPE_DEPTH_PREPASS;

    SceneData scene_data = {};
    scene_data.view = renderstate.camera_view;
    scene_data.proj = renderstate.fullscreen_proj;
    scene_data.view_proj = scene_data.proj * scene_data.view;
    UpdateGlobalSceneData(scene_data);
    
    PushConstant_PassHeader push_pass = {};  // No inputs, so doesn't care use push constant's upper bytes

    const uint32_t shader_id = SHADER_DEPTH;
    for (uint32_t i = 0; i < renderstate.drawcalls_collection.array[shader_id].drawcall_count; ++i)
    {
        DrawCall drawcall = renderstate.drawcalls_collection.array[shader_id].drawcalls[i];
        
        PipelineKey key = {
            .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
            .shader_id      = shader_id,
            .pass_type      = pass_type,
            .vertex_type    = (uint64_t)drawcall.renderable->mesh_prefab.vertex_type,
            .depth_test     = 1,
            .depth_write    = 1,
            .depth_op       = VK_COMPARE_OP_LESS,
            .stencil_mode   = 0,
            .cull_mode      = VK_CULL_MODE_BACK_BIT,
    #warning BLEND MODE MASKED TOO (TODO: Renderqueues n sort key shit)
            .blend_mode     = BLEND_MODE_OPAQUE,
            .polygon_mode   = VK_POLYGON_MODE_FILL,
            .front_face     = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .msaa_samples   = (uint64_t)PK_MultisamplingFlag(renderstate.multisampling_count_flag)
        };

        ExecuteDrawCall(cmd, drawcall, key, push_pass);
    }
}
