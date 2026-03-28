#include "drawcall.h"

#include "materials.h"
#include "internal_state.h"
#include "mapped_linear_allocator.h"
#include <glm/gtc/matrix_transform.hpp>  // lookAt, perspective, translate

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
        
        #warning TODO: Sorting should actually be in the transparent renderpasses execute callback
        .sort_depth = 0,

        // Push renderables transform to the mapped buffer and keep the pointer to it in draw calls.
        .object_ptr = PushToMappedArena(&renderstate.object_transforms, &r->transform, sizeof(r->transform)),
    };

    #warning NEED TO COPY CPU SIDE JOINTS BUFFER (r->joints) TO joints_buffer_rid
    #warning Probably implement it using a single fence in EndDrawCalls()?
    
    const MaterialPipelineInfo* const shaders_for_material = &g_material_configs.array[r->mesh_prefab.mat_type];

    // Submit draw with primary shader
    {
        uint32_t primary_shader_id = shaders_for_material->primary_shader_id;
        uint32_t* primary_shader_drawcall_count = &renderstate.drawcalls_collection.array[shaders_for_material->primary_shader_id].drawcall_count;

        SDL_assert(
            *primary_shader_drawcall_count + 1 < MAX_DRAWCALLS_PER_SHADER &&
            "If this is exceeded, either increase MAX_DRAWCALLS_PER_SHADER to a ludicrous value, or use a dynamic array."
        );

        renderstate.drawcalls_collection.array[primary_shader_id].drawcalls[
            (*primary_shader_drawcall_count)++
        ] = drawcall;
    }

    // Submit draw with secondary shader (Making sure it exists first)
    if (shaders_for_material->secondary_shader_id != SHADER_NONE)
    {
        uint32_t secondary_shader_id = shaders_for_material->secondary_shader_id;
        uint32_t* secondary_shader_drawcall_count = &renderstate.drawcalls_collection.array[shaders_for_material->secondary_shader_id].drawcall_count;
        SDL_assert(
            *secondary_shader_drawcall_count + 1 < MAX_DRAWCALLS_PER_SHADER &&
            "If this is exceeded, either increase MAX_DRAWCALLS_PER_SHADER to a ludicrous value, or use a dynamic array."
        );

        renderstate.drawcalls_collection.array[shaders_for_material->secondary_shader_id].drawcalls[
            (*secondary_shader_drawcall_count)++
        ] = drawcall;
    }
}


void ExecuteDrawCall(VkCommandBuffer cmd, DrawCall drawcall, PipelineKey key)
{
    // Get/Create and Bind Pipeline
    VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, key);
    if (renderstate.currently_bound_pipeline != pipeline)
    {
        renderstate.currently_bound_pipeline = pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    // ASSUMES: global texture descriptor set is already bound to VK_PIPELINE_BIND_POINT_GRAPHICS

    // Prepare Push Constants 
    GraphicsPushConstants pc = {};
    pc.scene_ptr    = renderstate.registry.resources[renderstate.rids.global_scene_buffer_rid].buffer_gpu_address;
    pc.material_ptr = renderstate.registry.resources[renderstate.rids.material_ssbo_rid].buffer_gpu_address;
    pc.object_ptr   = drawcall.object_ptr;
    if (drawcall.renderable->mesh_prefab.vertex_type == VERTEX_TYPE_SKINNED)
    {
        pc.joints_ptr = renderstate.registry.resources[
                        drawcall.renderable->mesh_prefab.mesh_rids.joints_buffer_rid
                    ].buffer_gpu_address;
    }
    else
    {
        pc.joints_ptr = 0;
    }

    for (uint32_t prim_i = 0; prim_i < drawcall.renderable->mesh_prefab.mesh_rids.primitive_count; ++prim_i)
    {
        PrimitiveRIDs* prim_rids = &drawcall.renderable->mesh_prefab.mesh_rids.primitives[prim_i];

        // Prepare per primitive Push Constants
        pc.material_idx     = prim_rids->material_index;
        pc.index_ptr        = renderstate.registry.resources[prim_rids->index_buf_rid].buffer_gpu_address;

        // Mandatory vertex attributes
        pc.v_positions_ptr  = renderstate.registry.resources[prim_rids->v_pos_buf_rid].buffer_gpu_address;
        pc.v_texcoords_ptr  = renderstate.registry.resources[prim_rids->v_texcoord_buf_rid].buffer_gpu_address;
        pc.v_normals_ptr    = renderstate.registry.resources[prim_rids->v_normal_buf_rid].buffer_gpu_address;

        // Optional vertex attributes:

        // TODO: Add tangents?

        if (prim_rids->v_color_buf_rid != UINT32_MAX)
            pc.v_colors_ptr = renderstate.registry.resources[prim_rids->v_color_buf_rid].buffer_gpu_address;
        else
            pc.v_colors_ptr = 0;

        if (prim_rids->v_joint_ids_buf_rid != UINT32_MAX)
            pc.v_joint_ids_ptr = renderstate.registry.resources[prim_rids->v_joint_ids_buf_rid].buffer_gpu_address;
        else
            pc.v_joint_ids_ptr = 0;

        if (prim_rids->v_joint_weights_buf_rid != UINT32_MAX)
            pc.v_joint_weights_ptr = renderstate.registry.resources[prim_rids->v_joint_weights_buf_rid].buffer_gpu_address;
        else
            pc.v_joint_weights_ptr = 0;


        vkCmdPushConstants(cmd, renderstate.global_pipeline_layout, 
                        VK_SHADER_STAGE_ALL, 
                        0, sizeof(GraphicsPushConstants), &pc);

        // Draw
        // NOTE: Since we use Vertex Pulling, we don't bind vertex or index buffers.
        //       In fact, here we gotta use vkCmdDraw, but with the index count.
        //       Not using vkCmdDrawIndexed because the index buffer is not the one part of pipeline pVertexInputState
        uint32_t index_count = renderstate.registry.resources[prim_rids->index_buf_rid].buffer.size / sizeof(uint32_t);
        uint32_t instance_count = 1;  // TODO: Instanced rendering

        vkCmdDraw(cmd, index_count, instance_count, 0, 0);
    }
}

    // NOTE: NEXT STEP IS MOVE THE STUFF BELOW TO A HELPER DRAW FUNCTION WE CALL IN RENDERPASS EXECUTE CALLBACKS
#if 0

    // TODO: Move Pipeline key creation to per pass.
    // Craft pipeline key from renderable
    PipelineKey key = {
        .pipeline_type  = PK_PIPELINE_TYPE_GRAPHICS,
        .shader_id      = shader_id,
        .pass_type      = PK_PIPELINE_TYPE_GRAPHICS,

        .vertex_type    = r->mesh_prefab.vertex_type,
        .depth_test     = 
        .depth_write    =
        .depth_op       =
        .stencil_mode   =
        .cull_mode      =
        .blend_mode     =
        .polygon_mode   =
        .front_face     =
    };
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

    // Get/Bind Pipeline
    VkPipeline pipeline = PK_GetOrCreatePipeline(&renderstate.pipeline_map, key);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind the Global Bindless Set (textures)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        renderstate.global_pipeline_layout, 0, 1, &renderstate.heap.global_set, 0, NULL
    );

    // Prepare Push Constants
    GraphicsPushConstants pc = {};
    pc.scene_ptr    = renderstate.registry.resources[renderstate.rids.global_scene_buffer_rid].buffer_gpu_address;
    pc.object_ptr   = r->object_ptr;
    pc.material_ptr = renderstate.registry.resources[renderstate.rids.material_ssbo_rid].buffer_gpu_address;
    pc.material_idx = r->material_idx;
    pc.joints_ptr    = r->vertex_type == VERTEX_TYPE_SKINNED ? r->joints_ptr : 0;

    pc.index_ptr    = renderstate.registry.resources[r->mesh_rids.index_buf_rid].buffer_gpu_address;
    pc.v_positions_ptr      = renderstate.registry.resources[r->mesh_rids.v_pos_buf_rid].buffer_gpu_address;
    pc.v_texcoords_ptr      = renderstate.registry.resources[r->mesh_rids.v_texcoord_buf_rid].buffer_gpu_address;
    pc.v_normals_ptr        = renderstate.registry.resources[r->mesh_rids.v_normal_buf_rid].buffer_gpu_address;

    // TODO: Remove vertex colors probably
    if (r->mesh_rids.v_color_buf_rid != UINT32_MAX)
        pc.v_colors_ptr         = renderstate.registry.resources[r->mesh_rids.v_color_buf_rid].buffer_gpu_address;
    else
        pc.v_colors_ptr = 0;

    if (r->mesh_rids.v_joint_ids_buf_rid != UINT32_MAX)
        pc.v_joint_ids_ptr = renderstate.registry.resources[r->mesh_rids.v_joint_ids_buf_rid].buffer_gpu_address;
    else
        pc.v_joint_ids_ptr = 0;

    if (r->mesh_rids.v_joint_weights_buf_rid != UINT32_MAX)
        pc.v_joint_weights_ptr = renderstate.registry.resources[r->mesh_rids.v_joint_weights_buf_rid].buffer_gpu_address;
    else
        pc.v_joint_weights_ptr = 0;


    vkCmdPushConstants(cmd, renderstate.global_pipeline_layout, 
                       VK_SHADER_STAGE_ALL, 
                       0, sizeof(GraphicsPushConstants), &pc);

    // Draw
    // NOTE: Since we use Vertex Pulling, we don't bind vertex or index buffers.
    //       In fact, here we gotta use vkCmdDraw, but with the index count.
    //       Not using vkCmdDrawIndexed because the index buffer is not the one part of pipeline pVertexInputState
    uint32_t index_count = renderstate.registry.resources[r->mesh_rids.index_buf_rid].buffer.size / sizeof(uint32_t);
    uint32_t instance_count = 1;  // TODO: Instanced rendering

    vkCmdDraw(cmd, index_count, instance_count, 0, 0);
}
#endif


// SCENE DATA UPDATION DURING FRAME

static SceneData temp_default_camera_scene_data()
{
    SceneData data = {};
    
    static glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);

    // Rotation state
    static float yaw   = -90.0f; // looking down -Z initially
    static float pitch =  0.0f;

    const bool* state = SDL_GetKeyboardState(NULL);

    float moveSpeed = 0.05f;
    float rotSpeed  = 1.5f; // degrees per frame (tweak as needed)

    // --- ROTATION (arrow keys) ---
    if (state[SDL_SCANCODE_LEFT])  yaw   -= rotSpeed;
    if (state[SDL_SCANCODE_RIGHT]) yaw   += rotSpeed;
    if (state[SDL_SCANCODE_UP])    pitch += rotSpeed;
    if (state[SDL_SCANCODE_DOWN])  pitch -= rotSpeed;

    // Clamp pitch to avoid flipping
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // --- DIRECTION VECTOR ---
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(forward);

    // --- MOVEMENT (WASD relative to camera) ---
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (state[SDL_SCANCODE_W]) pos += forward * moveSpeed;
    if (state[SDL_SCANCODE_S]) pos -= forward * moveSpeed;
    if (state[SDL_SCANCODE_A]) pos -= right   * moveSpeed;
    if (state[SDL_SCANCODE_D]) pos += right   * moveSpeed;

    // --- VIEW MATRIX ---
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    data.view = glm::lookAt(pos, pos + forward, up);

    // --- PROJECTION ---
    float fov = glm::radians(60.0f);
    float aspect = (float)renderstate.swapchain_extent.width / (float)renderstate.swapchain_extent.width;

    data.proj = glm::perspective(fov, aspect, 0.1f, 100.0f);
    // GLM is OpenGL-style by default:
    // - Y is flipped
    // - Z is -1..1 instead of 0..1
    data.proj[1][1] *= -1.0f;


    data.view_proj = data.proj * data.view;

    return data;
}

void UpdateGlobalSceneData(SceneData data)
{
    #warning SceneData argument currently ignored.

    data = temp_default_camera_scene_data();
    
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.global_scene_buffer_rid, 
                        &data, sizeof(SceneData)
    );
}
