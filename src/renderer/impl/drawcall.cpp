#include "drawcall.h"

#include "clustered_shading.h"
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
    ResetMappedArena(&renderstate.scenes_arena);
    ResetMappedArena(&renderstate.object_transforms);
    ResetMappedArena(&renderstate.joint_transforms);

    renderstate.drawcalls_collection.is_currently_adding_drawcalls = 1;
}

void EndDrawCalls()
{
    renderstate.drawcalls_collection.is_currently_adding_drawcalls = 0;

    // Upload lights
    FG_Resource* lights_header_buf = &renderstate.registry.resources[renderstate.rids.lights_header_buffer_rid];
    FG_Resource* pl_buf = &renderstate.registry.resources[renderstate.rids.point_lights_buffer_rid];
    FG_Resource* sl_buf = &renderstate.registry.resources[renderstate.rids.spot_lights_buffer_rid];
    FG_Resource* spotlight_shadowmap_id_buf_res     = &renderstate.registry.resources[renderstate.rids.spotlight_shadowmap_index_buffer_rid];
    FG_Resource* shadowmap_spotlight_camera_buf_res = &renderstate.registry.resources[renderstate.rids.shadowmap_spotlight_camera_buffer_rid];
    FG_Resource* point_light_indices_buffer_res = &renderstate.registry.resources[renderstate.rids.point_light_indices_buffer_rid];
    FG_Resource* spot_light_indices_buffer_res = &renderstate.registry.resources[renderstate.rids.spot_light_indices_buffer_rid];
    FG_Resource* cluster_offsets_buffer_rid = &renderstate.registry.resources[renderstate.rids.cluster_offsets_buffer_rid];

    LightsHeader header = {
        .num_point_lights  = renderstate.renderables_arena.num_point_lights,
        .num_spot_lights   = renderstate.renderables_arena.num_spot_lights,
        .point_lights_ptr  = pl_buf->buffer_gpu_address,
        .spot_lights_ptr   = sl_buf->buffer_gpu_address,
        .spotlight_shadowmap_index_buf_ptr = spotlight_shadowmap_id_buf_res->buffer_gpu_address,
        .shadowmap_spotlight_camera_buf_ptr = shadowmap_spotlight_camera_buf_res->buffer_gpu_address,
        .point_light_indices_buf_ptr = point_light_indices_buffer_res->buffer_gpu_address,
        .spot_light_indices_buf_ptr = spot_light_indices_buffer_res->buffer_gpu_address,
        .cluster_offsets_buf_ptr = cluster_offsets_buffer_rid->buffer_gpu_address,
    };
    
    vmaCopyMemoryToAllocation(renderstate.vma_allocator, &header, lights_header_buf->allocation, 0, sizeof(LightsHeader));
    vmaCopyMemoryToAllocation(renderstate.vma_allocator,
        renderstate.renderables_arena.point_lights, pl_buf->allocation, 0, sizeof(PointLight) * header.num_point_lights
    );
    vmaCopyMemoryToAllocation(renderstate.vma_allocator,
        renderstate.renderables_arena.spot_lights, sl_buf->allocation, 0, sizeof(SpotLight) * header.num_spot_lights
    );

    // Copy which lights are shadowed to the shadowed buffer
    int* mapped_shadowmap_ids_array = (int*)spotlight_shadowmap_id_buf_res->buffer.mapped_data;
    memset(mapped_shadowmap_ids_array, -1, sizeof(int) * renderstate.renderables_arena.num_spot_lights);

    uint32_t next_shadowmap_slot = 0;
    for (uint32_t i = 0; i < renderstate.num_shadowed_spotlights; ++i)
    {
        uint32_t index = renderstate.shadowed_spotlight_indices[i];
        mapped_shadowmap_ids_array[index] = next_shadowmap_slot++;
    }
    vmaFlushAllocation(
        renderstate.vma_allocator,
        spotlight_shadowmap_id_buf_res->allocation,
        0,
        sizeof(int) * renderstate.renderables_arena.num_spot_lights
    );

    // Calculate scene data for each spotlight shader map, which contains a view_proj matrix
    for (uint32_t i = 0; i < renderstate.num_shadowed_spotlights; ++i)
    {
        FG_Resource* shadowmap_res = &renderstate.registry.resources[renderstate.rids.shadow_map_rids[i]];

        renderstate.shadowed_spotlight_scenedatas[i] = MakeSpotLightSceneData(
            renderstate.renderables_arena.spot_lights[renderstate.shadowed_spotlight_indices[i]],
            (VkExtent2D){ shadowmap_res->image.extent.width, shadowmap_res->image.extent.height }
        );
    }

    // Copy the view_proj matrix for the spotlight shadow maps to the GPU buffer
    float* mapped_shadowmap_viewproj_matrices = (float*)shadowmap_spotlight_camera_buf_res->buffer.mapped_data;
    for (uint32_t i = 0; i < renderstate.num_shadowed_spotlights; ++i)
    {
        memcpy(
            mapped_shadowmap_viewproj_matrices + i * 16,
            renderstate.shadowed_spotlight_scenedatas[i].view_proj,
            sizeof(glm::mat4)
        );
    }
    vmaFlushAllocation(
        renderstate.vma_allocator,
        shadowmap_spotlight_camera_buf_res->allocation,
        0,
        sizeof(glm::mat4) * renderstate.num_shadowed_spotlights
    );

    // Create cluster grid, assign lights and upload that grid to the GPU
    ClusteredShading_CPULightAssignmentToMappedBuffer();
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

/*  NOTE:
    While keeping the drawcall API simple as it already is,
    under the hood we can do as many optimisations as needed.
        
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


    TODO: Whilst using glTF we gotta deal with meshes being multiple primitives.
    In the future, when reusing this with a custom format, decide whether this is still a good choice.
    (vs just allowing multiple mesh components, which would be better to allow mixed material types for one entity).
*/

typedef struct DrawPrimitive
{
    DrawCall dc;
    PipelineKey pipeline_key;
    uint32_t prim_idx;
    uint32_t sort_key;  // TODO unused
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

        // TODO: Sort by mesh/primitive rid
        //  Then we can use an instanced drawcall for the same meshes.
        //  Note however that things like grass should not be sorted this though
        //  as it's too expensive CPU side. Which is why I'd like to rework the
        //  mesh->primitive thing that glTF has made us use. (I.e. cut off glTF)
        //  This way we can design to minimize overhead for hugely batched draws like grass.

        return 0;
    }
}

void SortDraws(DrawPrimSortFunc sort_func)
{
    qsort(loaded_draws, num_loaded_draws, sizeof(DrawPrimitive), sort_func);
}

void ExecuteDraws(VkCommandBuffer cmd, PushConstant_PassHeader push_pass, uint64_t scene_ptr)
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
        push.dc.scene_ptr    = scene_ptr;
        push.dc.material_ptr = renderstate.registry.resources[renderstate.rids.materials_buffer_rid].buffer_gpu_address;
        push.dc.lights_header_ptr  = renderstate.registry.resources[renderstate.rids.lights_header_buffer_rid].buffer_gpu_address;
        push.dc.object_ptr   = draw->dc.object_ptr;
        push.dc.joints_ptr   = draw->dc.joints_ptr;

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

void ExecuteFullscreenPass(VkCommandBuffer cmd, uint32_t shader_id, PipelineKey key, PushConstant_PassHeader push_pass, uint64_t scene_ptr)
{
    VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, key);
    if (renderstate.currently_bound_pipeline != pipeline) 
    {
        renderstate.currently_bound_pipeline = pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    // Prepare the pass part of Push Constants
    // We only need the .pass part (texture/sampler indices)
    // And the scene data is so we can do special effects.
    FullPushConstants_Graphics push = {
        .dc = {
            .scene_ptr = scene_ptr
        },
        .pass = push_pass
    };

    vkCmdPushConstants(cmd, renderstate.global_pipeline_layout,
        VK_SHADER_STAGE_ALL, 0, sizeof(push), &push
    );

    // Push 3 empty vertices we'll use for a fullscreen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
