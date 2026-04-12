#include "drawcall.h"

#include "materials.h"
#include "internal_state.h"
#include "mapped_linear_allocator.h"
#include "glm/gtc/matrix_transform.hpp"
#include "core/my_c_runtime.h"

void InitDrawCallCollections()
{
    renderstate.drawcalls_collection = {};

    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        renderstate.drawcalls_collection.array[i].drawcalls = (DrawCall*)L_calloc(MAX_DRAWCALLS_PER_SHADER, sizeof(DrawCall), &renderstate.main.tt);
    }

    renderstate.drawcalls_collection.is_allocated = 1;
}

void DestroyDrawCallCollections()
{
    SDL_assert(renderstate.drawcalls_collection.is_allocated);

    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        L_free(renderstate.drawcalls_collection.array[i].drawcalls, &renderstate.main.tt);
    }

    renderstate.drawcalls_collection.is_allocated = 0;
}

void BeginDrawCalls()
{
    SDL_assert(renderstate.drawcalls_collection.is_allocated);

    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        // Move 'head' back to beginning (not zeroing the memory because it's unnecessary)
        renderstate.drawcalls_collection.array[i].drawcall_count = 0;
    }

    // Empty object data (e.g. model transforms)
    ResetMappedArena(&renderstate.object_transforms);
    ResetMappedArena(&renderstate.joint_transforms);

    renderstate.drawcalls_collection.is_currently_adding_drawcalls = 1;
}

void EndDrawCalls()
{
    renderstate.drawcalls_collection.is_currently_adding_drawcalls = 0;
}

void AddDrawCall(Renderable* r)
{
    SDL_assert(renderstate.drawcalls_collection.is_allocated);
    SDL_assert(renderstate.drawcalls_collection.is_currently_adding_drawcalls == 1
        && "Make sure you are not submitting draw calls outside of Begin/EndDrawCalls()"
    );

    DrawCall drawcall = {
        .renderable = r,

        // Push renderables transform to the mapped buffer and keep the pointer to it in draw calls.
        .object_ptr = PushToMappedArena(&renderstate.object_transforms, &r->transform, sizeof(r->transform)),
        .joints_ptr = r->joints ? PushToMappedArena(&renderstate.joint_transforms, r->joints, sizeof(glm::mat4) * r->joint_count) : 0
    };
    
    const MaterialPipelineInfo* const shaders_for_material = &g_material_configs.array[r->mesh_prefab.mat_type];

    // Submit draw with depth prepasss
    {
        // TODO: Should I ALWAYS be depth prepassing? It may break under some materials that displace vertices in the vertex shader
        uint32_t shader_id = SHADER_DEPTH;
        uint32_t* shader_drawcall_count = &renderstate.drawcalls_collection.array[shader_id].drawcall_count;

        SDL_assert(
            *shader_drawcall_count + 1 < MAX_DRAWCALLS_PER_SHADER &&
            "If this is exceeded, either increase MAX_DRAWCALLS_PER_SHADER to a ludicrous value, or use a dynamic array."
        );

        renderstate.drawcalls_collection.array[shader_id].drawcalls[
            (*shader_drawcall_count)++
        ] = drawcall;
    }

    // Submit draw with primary shader
    {
        uint32_t shader_id = shaders_for_material->primary_shader_id;
        uint32_t* shader_drawcall_count = &renderstate.drawcalls_collection.array[shader_id].drawcall_count;

        SDL_assert(
            *shader_drawcall_count + 1 < MAX_DRAWCALLS_PER_SHADER &&
            "If this is exceeded, either increase MAX_DRAWCALLS_PER_SHADER to a ludicrous value, or use a dynamic array."
        );

        renderstate.drawcalls_collection.array[shader_id].drawcalls[
            (*shader_drawcall_count)++
        ] = drawcall;
    }

    // Submit draw with secondary shader (Making sure it exists first)
    if (shaders_for_material->secondary_shader_id != SHADER_NONE)
    {
        uint32_t shader_id = shaders_for_material->secondary_shader_id;
        uint32_t* shader_drawcall_count = &renderstate.drawcalls_collection.array[shader_id].drawcall_count;

        SDL_assert(
            *shader_drawcall_count + 1 < MAX_DRAWCALLS_PER_SHADER &&
            "If this is exceeded, either increase MAX_DRAWCALLS_PER_SHADER to a ludicrous value, or use a dynamic array."
        );

        renderstate.drawcalls_collection.array[shader_id].drawcalls[
            (*shader_drawcall_count)++
        ] = drawcall;
    }
}


// DRAW CALL SORTING AND EXECUTION

/*  NOTE(Liam):
    While keeping the drawcall API simple as it already is,
    under the hood we can do as many optimisations as needed.
    (because it's not OOP template metaprogrammed c++ inspired dog)
    
    In this case, our renderables are made up of many primitives,
    these primitives may have different pipeline state to each other.
    For instance, alpha blending vs masked vs opaque.

    So under the hood, our 'atomic' draw call unit is for a primitive.
    A primitive gets the actual pipeline associated with it then.
    And we can also optimise draw order based on sort key.

    Draw order:
    - Most important: limit the number of pipeline changes,
      some pipeline state changes are extremely expensive.
    - Depth sort: The depth prepass is faster front to back, 
      also alpha masked geometry should be the done lastly in the depth prepass.

*/

typedef struct DrawPrimitive
{
    DrawCall dc;
    PipelineKey pipeline_key;
    uint32_t prim_idx;
    uint32_t sort_key;  // TODO
}
DrawPrimitive;

uint32_t num_loaded_draws = 0;
MY_THREAD_LOCAL DrawPrimitive loaded_draws[MAX_DRAWCALLS_PER_SHADER];

void ResetDrawArena()
{
    num_loaded_draws = 0;    
}

void PushDrawPrimitive(DrawCall dc, PipelineKey pipeline_key, uint32_t prim_idx, uint32_t sort_key)
{
    loaded_draws[num_loaded_draws++] = {
        .dc           = dc,
        .pipeline_key = pipeline_key,
        .prim_idx     = prim_idx,
        .sort_key     = sort_key
    };
}

int DrawPrimSortFunc_Default(const void* a, const void* b)
{
    const DrawPrimitive* prim_a = (const DrawPrimitive*)a;
    const DrawPrimitive* prim_b = (const DrawPrimitive*)b;
    // First sorting by pipeline key because changing pipelines is expensive
    if (prim_a->pipeline_key.value > prim_b->pipeline_key.value)
    {
        return 1;
    }
    else if (prim_a->pipeline_key.value < prim_b->pipeline_key.value)
    {
        return -1;
    }
    else
    {
        // TODO: Sort by quantized depth too.
        // And maybe some passes will want to prioritize depth over pipeline key
        // And others may want to only do alpha masked geometry last, etc.
        return 0;
    }
}

void SortDraws(DrawPrimSortFunc sort_func)
{
    qsort(loaded_draws, num_loaded_draws, sizeof(DrawPrimitive), sort_func);
}

void ExecuteDraws(VkCommandBuffer cmd, PushConstant_PassHeader push_pass)
{
    for (uint32_t i = 0; i < num_loaded_draws; ++i)
    {
        DrawPrimitive* draw = &loaded_draws[i];
        PrimitiveRIDs* prim_rids = &draw->dc.renderable->mesh_prefab.mesh_rids.primitives[draw->prim_idx];

        // ASSUMES: global texture descriptor set is already bound to VK_PIPELINE_BIND_POINT_GRAPHICS

        // Get/Create and Bind Pipeline
        VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, draw->pipeline_key);
        if (renderstate.currently_bound_pipeline != pipeline)  // <- No redundant ass pipeline binds
        {
            renderstate.currently_bound_pipeline = pipeline;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        FullPushConstants_Graphics push = {
            .dc = {},
            .pass = push_pass
        };

        // Prepare the draw call part of Push Constants 
        push.dc.scene_ptr    = renderstate.registry.resources[renderstate.rids.global_scene_buffer_rid].buffer_gpu_address;
        push.dc.material_ptr = renderstate.registry.resources[renderstate.rids.material_ssbo_rid].buffer_gpu_address;
        push.dc.object_ptr   = draw->dc.object_ptr;
        push.dc.joints_ptr = draw->dc.joints_ptr;

        // Prepare push constants draw call section
        push.dc.material_idx     = prim_rids->material_index;
        push.dc.index_ptr        = renderstate.registry.resources[prim_rids->index_buf_rid].buffer_gpu_address;

        // Mandatory vertex attributes
        push.dc.v_positions_ptr  = renderstate.registry.resources[prim_rids->v_pos_buf_rid].buffer_gpu_address;
        push.dc.v_texcoords_ptr  = renderstate.registry.resources[prim_rids->v_texcoord_buf_rid].buffer_gpu_address;
        push.dc.v_normals_ptr    = renderstate.registry.resources[prim_rids->v_normal_buf_rid].buffer_gpu_address;

        // Optional vertex attributes:

        // TODO: Add tangents?

        if (prim_rids->v_color_buf_rid != UINT32_MAX)
            push.dc.v_colors_ptr = renderstate.registry.resources[prim_rids->v_color_buf_rid].buffer_gpu_address;
        else
            push.dc.v_colors_ptr = 0;

        if (prim_rids->v_joint_ids_buf_rid != UINT32_MAX)
            push.dc.v_joint_ids_ptr = renderstate.registry.resources[prim_rids->v_joint_ids_buf_rid].buffer_gpu_address;
        else
            push.dc.v_joint_ids_ptr = 0;

        if (prim_rids->v_joint_weights_buf_rid != UINT32_MAX)
            push.dc.v_joint_weights_ptr = renderstate.registry.resources[prim_rids->v_joint_weights_buf_rid].buffer_gpu_address;
        else
            push.dc.v_joint_weights_ptr = 0;


        vkCmdPushConstants(cmd, renderstate.global_pipeline_layout,
            VK_SHADER_STAGE_ALL, 0, sizeof(push), &push
        );

        // Draw
        // NOTE: Since we use Vertex Pulling, we don't bind vertex or index buffers.
        //       In fact, here we gotta use vkCmdDraw, but with the index count.
        //       Not using vkCmdDrawIndexed because the index buffer is not the one part of pipeline pVertexInputState
        uint32_t index_count = renderstate.registry.resources[prim_rids->index_buf_rid].buffer.size / sizeof(uint32_t);
        uint32_t instance_count = 1;  // TODO: Instanced rendering

        vkCmdDraw(cmd, index_count, instance_count, 0, 0);
    }
}


#warning IMPLEMENT NEW STUFF THEN REMOVE THE BELOW
#if 0
void ExecuteDrawCall(VkCommandBuffer cmd, DrawCall drawcall, PipelineKey key, PushConstant_PassHeader push_pass)
{
    // ASSUMES: global texture descriptor set is already bound to VK_PIPELINE_BIND_POINT_GRAPHICS

    // Get/Create and Bind Pipeline
    VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, key);
    if (renderstate.currently_bound_pipeline != pipeline)  // <- No redundant ass pipeline binds
    {
        renderstate.currently_bound_pipeline = pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    FullPushConstants_Graphics push = {
        .dc = {},
        .pass = push_pass
    };

    // Prepare the draw call part of Push Constants 
    push.dc.scene_ptr    = renderstate.registry.resources[renderstate.rids.global_scene_buffer_rid].buffer_gpu_address;
    push.dc.material_ptr = renderstate.registry.resources[renderstate.rids.material_ssbo_rid].buffer_gpu_address;
    push.dc.object_ptr   = drawcall.object_ptr;
    push.dc.joints_ptr = drawcall.joints_ptr;

    for (uint32_t prim_i = 0; prim_i < drawcall.renderable->mesh_prefab.mesh_rids.primitive_count; ++prim_i)
    {
        PrimitiveRIDs* prim_rids = &drawcall.renderable->mesh_prefab.mesh_rids.primitives[prim_i];

        // Prepare per primitive Push Constants
        push.dc.material_idx     = prim_rids->material_index;
        push.dc.index_ptr        = renderstate.registry.resources[prim_rids->index_buf_rid].buffer_gpu_address;

        // Mandatory vertex attributes
        push.dc.v_positions_ptr  = renderstate.registry.resources[prim_rids->v_pos_buf_rid].buffer_gpu_address;
        push.dc.v_texcoords_ptr  = renderstate.registry.resources[prim_rids->v_texcoord_buf_rid].buffer_gpu_address;
        push.dc.v_normals_ptr    = renderstate.registry.resources[prim_rids->v_normal_buf_rid].buffer_gpu_address;

        // Optional vertex attributes:

        // TODO: Add tangents?

        if (prim_rids->v_color_buf_rid != UINT32_MAX)
            push.dc.v_colors_ptr = renderstate.registry.resources[prim_rids->v_color_buf_rid].buffer_gpu_address;
        else
            push.dc.v_colors_ptr = 0;

        if (prim_rids->v_joint_ids_buf_rid != UINT32_MAX)
            push.dc.v_joint_ids_ptr = renderstate.registry.resources[prim_rids->v_joint_ids_buf_rid].buffer_gpu_address;
        else
            push.dc.v_joint_ids_ptr = 0;

        if (prim_rids->v_joint_weights_buf_rid != UINT32_MAX)
            push.dc.v_joint_weights_ptr = renderstate.registry.resources[prim_rids->v_joint_weights_buf_rid].buffer_gpu_address;
        else
            push.dc.v_joint_weights_ptr = 0;


        vkCmdPushConstants(cmd, renderstate.global_pipeline_layout,
            VK_SHADER_STAGE_ALL, 0, sizeof(push), &push
        );

        // Draw
        // NOTE: Since we use Vertex Pulling, we don't bind vertex or index buffers.
        //       In fact, here we gotta use vkCmdDraw, but with the index count.
        //       Not using vkCmdDrawIndexed because the index buffer is not the one part of pipeline pVertexInputState
        uint32_t index_count = renderstate.registry.resources[prim_rids->index_buf_rid].buffer.size / sizeof(uint32_t);
        uint32_t instance_count = 1;  // TODO: Instanced rendering

        vkCmdDraw(cmd, index_count, instance_count, 0, 0);
    }
}
#endif

void ExecuteFullscreenPass(VkCommandBuffer cmd, uint32_t shader_id, PipelineKey key, PushConstant_PassHeader push_pass)
{
    VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, key);
    if (renderstate.currently_bound_pipeline != pipeline) 
    {
        renderstate.currently_bound_pipeline = pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    // Prepare the pass part of Push Constants
    // We only need the .pass part (texture/sampler indices)
    // TODO: If it turns I'm never using both dc and pass, then I can remove one of them
    //       This would half the push constants size requirements from 256 to 128
    FullPushConstants_Graphics push = {
        .dc = {}, 
        .pass = push_pass
    };

    vkCmdPushConstants(cmd, renderstate.global_pipeline_layout,
        VK_SHADER_STAGE_ALL, 0, sizeof(push), &push
    );

    // Push 3 empty vertices we'll use for a fullscreen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);
}


// SCENE DATA UPDATION DURING FRAME
// E.g. different renderpasses have different cameras, and other scene information.
// Such as forward pass vs shadow mapping.

void UpdateGlobalSceneData(SceneData data)
{
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.global_scene_buffer_rid, 
                        &data, sizeof(SceneData)
    );
}
