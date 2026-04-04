#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void ForwardOpaque_Execute(VkCommandBuffer cmd, RenderPassDesc* desc)
{
    const uint32_t pass_type = desc->pass_type;

    SceneData scene_data = {};
    scene_data.view = renderstate.camera_view;
    scene_data.proj = renderstate.fullscreen_proj;
    scene_data.view_proj = scene_data.proj * scene_data.view;
    UpdateGlobalSceneData(scene_data);
    
    PushConstant_PassHeader push_pass = {};

    uint32_t forward_shaders[] = { SHADER_UNLIT };

    for (uint32_t s = 0; s < sizeof(forward_shaders)/sizeof(forward_shaders[0]); ++s)
    {
        const uint32_t shader_id = forward_shaders[s];
        for (uint32_t i = 0; i < renderstate.drawcalls_collection.array[shader_id].drawcall_count; ++i)
        {
            DrawCall drawcall = renderstate.drawcalls_collection.array[shader_id].drawcalls[i];

            PipelineKey key = {
                .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
                .shader_id      = shader_id,
                .pass_type      = pass_type,
                .vertex_type    = drawcall.renderable->mesh_prefab.vertex_type,
                .depth_test     = 1,
                .depth_write    = 0,  // <- Shouldn't need to depth write since opaque is covered by depth prepass
                .depth_op       = VK_COMPARE_OP_EQUAL,  // <- Equal prolly good bcuz of invariant gl_Position in shaders
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
}
