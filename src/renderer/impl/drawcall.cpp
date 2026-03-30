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
        
        #warning TODO: Sorting should actually be in the transparent renderpasses execute callback
        .sort_depth = 0,

        // Push renderables transform to the mapped buffer and keep the pointer to it in draw calls.
        .object_ptr = PushToMappedArena(&renderstate.object_transforms, &r->transform, sizeof(r->transform)),
        .joints_ptr = r->joints ? PushToMappedArena(&renderstate.joint_transforms, r->joints, sizeof(glm::mat4) * r->joint_count) : 0
    };
    
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
            VK_SHADER_STAGE_ALL, 0, sizeof(push), &push.dc
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


// SCENE DATA UPDATION DURING FRAME

static SceneData temp_default_camera_scene_data()
{
    SceneData data = {};
    
    static glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);

    // Rotation state
    static float yaw   = -90.0f;  // Looking down -Z initially
    static float pitch =  0.0f;

    const bool* state = SDL_GetKeyboardState(NULL);

    float move_speed = 0.05f;
    float rot_speed  = 1.5f;  // Degrees per frame

    // --- ROTATION (arrow keys) ---
    if (state[SDL_SCANCODE_LEFT])  yaw   -= rot_speed;
    if (state[SDL_SCANCODE_RIGHT]) yaw   += rot_speed;
    if (state[SDL_SCANCODE_UP])    pitch += rot_speed;
    if (state[SDL_SCANCODE_DOWN])  pitch -= rot_speed;

    // Clamp pitch to avoid flipping
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // --- DIRECTION VECTOR ---
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- MOVEMENT (WASD relative to camera) ---
    if (state[SDL_SCANCODE_W]) pos += forward * move_speed;
    if (state[SDL_SCANCODE_S]) pos -= forward * move_speed;
    if (state[SDL_SCANCODE_A]) pos -= right   * move_speed;
    if (state[SDL_SCANCODE_D]) pos += right   * move_speed;
    if (state[SDL_SCANCODE_SPACE]) pos += up  * move_speed;
    if (state[SDL_SCANCODE_LSHIFT]) pos -= up  * move_speed;

    // --- VIEW MATRIX ---
    data.view = glm::lookAt(pos, pos + forward, up);

    // --- PROJECTION ---
    float fov = glm::radians(60.0f);
    float aspect = (float)renderstate.swapchain_extent.width / (float)renderstate.swapchain_extent.height;

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
