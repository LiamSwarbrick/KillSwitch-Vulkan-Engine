#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"

void ForwardOpaque_Execute(VkCommandBuffer cmd, uint32_t pass_idx)
{
    RenderPassDesc* desc = &renderstate.framegraph.passes[pass_idx];
    ForwardPass_UserData* user_data = (ForwardPass_UserData*)desc->user_data;

    uint64_t scene_ptr = 0;
    {
        VkExtent3D extents = renderstate.registry.resources[renderstate.rids.hdr_color_target_rid].image.extent;
        SceneData scene_data = MakeSceneData(renderstate.main_camera, (VkExtent2D){ extents.width, extents.height });
        scene_ptr = PushToMappedArena(&renderstate.scenes_arena, &scene_data, sizeof(SceneData));
    }

    uint32_t forward_shaders[] = { SHADER_UNLIT, SHADER_LIT, SHADER_OUTLINE };
    PushConstant_PassHeader push_pass = user_data->push_pass;

    ResetDrawArena();
    
    // For each shader s, drawcall i, primitive p:
    //     PushDrawPrimitive()
    for (uint32_t s = 0; s < sizeof(forward_shaders)/sizeof(forward_shaders[0]); ++s)
    {
        const uint32_t shader_id = forward_shaders[s];
        for (uint32_t i = 0; i < renderstate.drawcalls_collection.array[shader_id].drawcall_count; ++i)
        {
            DrawCall drawcall = renderstate.drawcalls_collection.array[shader_id].drawcalls[i];

            for (uint32_t p = 0; p < drawcall.renderable->mesh_prefab.mesh_rids.primitive_count; ++p)
            {
                PrimitiveRIDs* prim = &drawcall.renderable->mesh_prefab.mesh_rids.primitives[p];
                MaterialData* mat = &((MaterialData*)renderstate.registry.resources[renderstate.rids.materials_buffer_rid].buffer.mapped_data)[prim->material_index];

                PipelineKey key = {
                    .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
                    .shader_id      = shader_id,
                    .pass_idx       = pass_idx,
                    .vertex_type    = (uint64_t)drawcall.renderable->mesh_prefab.vertex_type,
                    .depth_test     = 1,
                    .depth_write    = 0,  // NOTE: Relying on depth prepass
                    .depth_op       = VK_COMPARE_OP_LESS_OR_EQUAL,  // <- Equal prolly good bcuz of invariant gl_Position in shaders
                    .stencil_mode   = 0,
                    .cull_mode      = VK_CULL_MODE_BACK_BIT,
                    .blend_mode     = mat->blend_mode,
                    .polygon_mode   = VK_POLYGON_MODE_FILL,
                    .front_face     = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .msaa_samples   = (uint64_t)PK_MultisamplingFlag(renderstate.multisampling_count_flag)
                };

                if (shader_id == SHADER_OUTLINE)
                {
                    key.cull_mode = VK_CULL_MODE_FRONT_BIT;
                }

                // TODO: Use sort key
                PushDrawPrimitive(drawcall, key, p, 0);
            }
        }
    }

    SortDraws(DrawPrimSortFunc_Default);
    ExecuteDraws(cmd, push_pass, scene_ptr);
}