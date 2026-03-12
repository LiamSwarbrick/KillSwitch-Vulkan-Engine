#include "../framegraph.h"
#include "../internal_state.h"
#include "../../render_types.h"
#include "glm/glm.hpp"
#include <math.h>

void SwapchainPass_Execute(VkCommandBuffer cmd, void* user_data)
{
    // Hardcode our Test Renderable
    Renderable tri = {};
    tri.mesh_rid    = renderstate.rids.test_triangle_rid;
    tri.material_id = 0;  // The white material we created in CreateOrRecreateResources
    
    ObjectData tri_object_data = { glm::mat4(1.0f) };
    tri_object_data.model[3][0] = sinf((float)renderstate.frame_number / 60.0f);
    
    tri.object_ptr  = push_object(&renderstate.object_transforms, tri_object_data);
    tri.joint_ptr   = 0;
    tri.vertex_type = VERTEX_TYPE_STATIC;
    tri.mat_type    = MAT_UNLIT;

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
