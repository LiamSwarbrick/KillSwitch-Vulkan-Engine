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

void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
{
    UpdateGlobalSceneData({});  // TODO: Set default camera in renderstate somewhere at some point, instead of it being inside UpdateGlobalSceneData
                                // E.g. for shadow passes, this function will be set from the lights perspective.
                                // It can be updated multiple times within one pass.


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
        #warning this is where i left off
        // TODO: add draw call, then make sure depth buffer is working, and remind myself how the framegraph inputs
        // should work
        ExecuteDrawCall(cmd, drawcall, key);
    }


#if 0

    // TODO: The pass itself should set the camera matrices
    // Because shadow passes and ui passes don't use the same camera    
    // OLD SHIT: Move this to renderer.cpp where the warning is about drawcalls
    int N = 1;
    for (int i = 0; i < N; ++i)
    {
        // TEMP: Hardcode our Test Renderable
        // ALSO TODO: Use depth buffer, and amend pipeline key.        
        ObjectData tri_object_data = { glm::mat4(1.0f) };

        // Renderable tri = {
        //     .vertex_type = VERTEX_TYPE_STATIC,
        //     .mat_type    = MAT_UNLIT,
        //     .sort_depth  = 0,  // <- NOTE: unused at the moment.

        //     // .mesh_rids = renderstate.rids.dummy_mesh,
        //     .mesh_rids = renderstate.rids.temp_test_mesh,
        //     .material_idx = 0,
        //     .object_ptr = PushToMappedArena(&renderstate.object_transforms, &tri_object_data, sizeof(tri_object_data)),
        //     .joint_ptr = 0
        // };

        #warning Pipeline key currently defined here, but instead renderables (see comment)
        /*
            Renderables should be added to drawcall arrays, and then we use the material's
            type (see materials.h/cpp) to add it to arrays for each shader relevant shader type.
            I.e.

            Renderable R with material type: MAT_PBR_WITH_OUTLINE:
            - primary shader id is SHADER_PBR
            - secondary shader id is SHADER_OUTLINE
            So SubmitDraw for R should add two DrawCalls.

            HOLD UP I'm just gonna write about this at the top
        */

        // Define the Pipeline State (The "Key")
        // This identifies which PSO to pull from the cache (or create)
        PipelineKey key = { 0 };
        key.pipeline_type = PK_PIPELINE_TYPE_GRAPHICS;
        key.shader_id     = SHADER_UNLIT;
        key.pass_type     = PASS_TYPE_SWAPCHAIN_PASS;
        key.vertex_type   = drawcall.mesh_prefab.vertex_type;
        key.depth_test    = 0;  // Triangle is 2D clip-space, no depth needed for test
        key.depth_write   = 0;
        key.depth_op      = VK_COMPARE_OP_NEVER;  // TODO.
        key.stencil_mode  = 0;
        key.cull_mode     = VK_CULL_MODE_NONE;  // No culling to ensure it shows regardless of winding
        key.blend_mode    = BLEND_MODE_OPAQUE;
        key.polygon_mode  = VK_POLYGON_MODE_FILL;
        key.front_face    = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Submit the Draw
        SubmitDraw(cmd, &tri, key);
    }
#endif
}
