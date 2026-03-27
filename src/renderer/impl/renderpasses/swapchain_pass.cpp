#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
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
    // TODO: The pass itself should set the camera matrices
    // Because shadow passes and ui passes don't use the same camera

    // for loop for unlit shader for now
    // TODO: During week where we integrate with entity system.
    
    // OLD SHIT: Move this to renderer.cpp where the warning is about drawcalls
    int N = 1;
    for (int i = 0; i < N; ++i)
    {
        // TEMP: Hardcode our Test Renderable
        // ALSO TODO: Use depth buffer, and amend pipeline key.        
        ObjectData tri_object_data = { glm::mat4(1.0f) };
        // tri_object_data.model[0][0] *= 2.0f/(float)N;
        // tri_object_data.model[1][1] *= 4.0f/(float)N;
        // tri_object_data.model[3][1] = -1.0f + 2.0f*(float)i/(float)N;
        // tri_object_data.model[3][0] = -1.0f + ((float)(N-1)/(float)N)*(1.0f + sinf(2.0f*(float)M_PI*(((float)i/(float)N) + (float)(renderstate.frame_number) / 600.0f)));
        // tri_object_data.model[0][0] *= fabsf(-1.0f + 8.0f * tri_object_data.model[3][0]);
    
        size_t joint_count = 164;
        glm::mat4* joint_matrices = (glm::mat4*)malloc(joint_count * sizeof(glm::mat4));
        for (size_t j = 0; j < joint_count; ++j) {
            joint_matrices[j] = glm::mat4(1.0f);
        }


        Renderable tri = {
            .vertex_type = VERTEX_TYPE_SKINNED,
            .mat_type    = MAT_UNLIT,
            .sort_depth  = 0,  // <- NOTE: unused at the moment.

            // .mesh_rids = renderstate.rids.dummy_mesh,
            .mesh_rids = renderstate.rids.temp_test_mesh,
            .material_idx = 0,
            .object_ptr = PushToMappedArena(&renderstate.object_transforms, &tri_object_data, sizeof(tri_object_data)),
            .joint_ptr = PushToMappedArena(&renderstate.object_transforms, joint_matrices, joint_count * sizeof(glm::mat4))
        };

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
        key.vertex_type   = tri.vertex_type;
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
}
