#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"

void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
{
    // Hardcode our Test Renderable
    Renderable tri = {};
    tri.mesh_rid    = renderstate.rids.test_triangle_rid;
    tri.material_id = 0; // The white material we created in CreateOrRecreateResources
    
    // Get the GPU address for our identity matrix
    // tri.object_ptr  = get_resource_buffer_device_address(renderstate.rids.identity_matrix_rid);
    tri.joint_ptr   = 0;
    
    tri.vertex_type = VERTEX_TYPE_STATIC;
    tri.mat_type    = MAT_UNLIT;

    // Define the Pipeline State (The "Key")
    // This identifies which PSO to pull from the cache (or create)
    PipelineKey key = {0};
    key.pipeline_type = PK_PIPELINE_TYPE_GRAPHICS;
    key.shader_id     = SHADER_UNLIT;
    key.pass_type     = PASS_TYPE_SWAPCHAIN_PASS;
    
    key.vertex_type   = tri.vertex_type;
    key.depth_test    = 0; // Triangle is 2D clip-space, no depth needed for test
    key.depth_write   = 0;
    key.depth_op      = VK_COMPARE_OP_NEVER;  // TODO.
    key.stencil_mode  = 0;
    key.cull_mode     = VK_CULL_MODE_NONE; // No culling to ensure it shows regardless of winding
    key.blend_mode    = BLEND_MODE_OPAQUE;
    key.polygon_mode  = VK_POLYGON_MODE_FILL;
    key.front_face    = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Submit the Draw
    SubmitDraw(cmd, &tri, key);
}

// void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
// {
//     RenderView view = {};
//     #warning Implement gather entities into renderview

//     Renderable tri = {};
//     tri.mesh_rid = renderstate.rids.test_triangle_rid;
//     tri.material_id = 0;
//     tri.object_ptr = 

//     tri.type = MAT_DEFAULT; // This will map to SHADER_UNLIT in our internal config
//     tri.material_id = 0;    // Assume we have a dummy material at 0
//     tri.object_data_gpu_address = 0;

//     // Bind the Global Bindless Set (Binding 0: Textures, Binding 1: Samplers)
//     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
//         renderstate.global_pipeline_layout, 0, 1, &renderstate.heap.global_set, 0, NULL
//     );

//     // printf("DEBUG: SwapchainPass_Execute called via function pointer: TODO: Shaders and drawing\n");

//     #warning SwapchainPass_Execute implement pipeline.
//     #if 0  // TODO: Implement temp pipeline:
//     // NOTE: Temporary pipeline to get something on the screen.
//     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderstate.temp_pipeline);

//     // Push Constants (Vertex Pulling Address)
//     // For this test, we'll assume we have a buffer resource for our vertices
//     FG_Resource* v_res = &renderstate.registry.resources[renderstate.rids.test_v_buffer_rid];
    
//     struct {
//         uint64_t global_ptr;
//         uint64_t vertex_ptr;
//     } pc = { 0, v_res->buffer_gpu_address };

//     vkCmdPushConstants(cmd, renderstate.global_pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(pc), &pc);

//     // Draw 3 vertices for a triangle
//     vkCmdDraw(cmd, 3, 1, 0, 0);
//     #endif
// }
