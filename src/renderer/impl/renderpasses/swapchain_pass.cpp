#include "../framegraph.h"
#include "../internal_state.h"

void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
{
    // Bind the Global Bindless Set (Binding 0: Textures, Binding 1: Samplers)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderstate.global_pipeline_layout, 0, 1, &renderstate.heap.global_set, 0, NULL
    );

    // printf("DEBUG: SwapchainPass_Execute called via function pointer: TODO: Shaders and drawing\n");

    #if 0  // TODO: Implement temp pipeline:
    // NOTE: Temporary pipeline to get something on the screen.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderstate.temp_pipeline);

    // Push Constants (Vertex Pulling Address)
    // For this test, we'll assume we have a buffer resource for our vertices
    FG_Resource* v_res = &renderstate.registry.resources[renderstate.rids.test_v_buffer_rid];
    
    struct {
        uint64_t global_ptr;
        uint64_t vertex_ptr;
    } pc = { 0, v_res->buffer_gpu_address };

    vkCmdPushConstants(cmd, renderstate.global_pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(pc), &pc);

    // Draw 3 vertices for a triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);
    #endif
}
