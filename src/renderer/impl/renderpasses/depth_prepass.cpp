#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void DepthPrepass_Execute(VkCommandBuffer cmd, void* user_data)
{
    UpdateGlobalSceneData({});

    // No inputs, so doesn't care use push constant's upper bytes
    PushConstant_PassHeader push_pass = {};

    const uint32_t shader_id = SHADER_DEPTH;
    const uint32_t pass_type = PASS_TYPE_DEPTH_PREPASS;
    for (uint32_t i = 0; i < renderstate.drawcalls_collection.array[shader_id].drawcall_count; ++i)
    {
        DrawCall drawcall = renderstate.drawcalls_collection.array[shader_id].drawcalls[i];
        
        PipelineKey key = {
            .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
            .shader_id      = shader_id,
            .pass_type      = pass_type,
            .vertex_type    = drawcall.renderable->mesh_prefab.vertex_type,
            .depth_test     = 1,
            .depth_write    = 1,
            .depth_op       = VK_COMPARE_OP_LESS,
            .stencil_mode   = 0,
            .cull_mode      = VK_CULL_MODE_BACK_BIT,
            .blend_mode     = BLEND_MODE_OPAQUE,
            .polygon_mode   = VK_POLYGON_MODE_FILL,
            .front_face     = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .msaa_samples   = PK_MultisamplingFlag(renderstate.multisampling_count_flag)
        };

        ExecuteDrawCall(cmd, drawcall, key, push_pass);
    }
}
