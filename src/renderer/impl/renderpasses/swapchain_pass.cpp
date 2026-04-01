#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "shaders.h"
#include "glm/glm.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

/*
TODO IMPORTANT:
Remember, framegraph has a bunch of renderpasses.
Each renderpass executes the draws for that specific shader.

SubmitDrawCalls
{
    Renderables 
}

E.g.
PBR_Opaque_Pass_Execute()
{
    for all 
}
*/

// Temp pass for now while i get stuff working
void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
{
    UpdateGlobalSceneData({});  // TODO: Set default camera in renderstate somewhere at some point, instead of it being inside UpdateGlobalSceneData
                                // E.g. for shadow passes, this function will be set from the lights perspective.
                                // It can be updated multiple times within one pass.

    // NOTE: No specific pass header for unlit forward pass, because it reads no texture.
    // But when adding deferred lighting: it will need the texture and sampler indices to use
    // for sampling the g-buffers. (Similarly for postprocess passes).
    PushConstant_PassHeader push_pass = {};

    // Draw unlit
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
            #warning As soon as drawcall works, put this into an opaque renderpass before swapchain pass, and this pass will have a depth buffer
            .depth_test     = 0,  // TODO See warning above
            .depth_write    = 0,  // TODO See warning above
            .depth_op       = VK_COMPARE_OP_NEVER,  // TODO See above warning/
            .stencil_mode   = 0,
            .cull_mode      = VK_CULL_MODE_BACK_BIT,
            .blend_mode     = BLEND_MODE_OPAQUE,
            .polygon_mode   = VK_POLYGON_MODE_FILL,  // <- NOTE: When doing debug draw, can use lines
            .front_face     = VK_FRONT_FACE_COUNTER_CLOCKWISE
        };

        ExecuteDrawCall(cmd, drawcall, key, push_pass);
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
