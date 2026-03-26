#include "shaders.h"
#include "internal_state.h"

#include <glm/gtc/matrix_transform.hpp>  // lookAt, perspective, translate

void UpdateGlobalSceneData()
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
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.global_scene_buffer_rid, 
                        &data, sizeof(SceneData)
    );

    // Object transforms
    ResetMappedArena(&renderstate.object_transforms);
}

void SubmitDraw(VkCommandBuffer cmd, Renderable* r, PipelineKey key)
{
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
    pc.joint_ptr    = r->vertex_type == VERTEX_TYPE_SKINNED ? r->joint_ptr : 0;

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

// Shader Registry
//

#define SHADER_SPIRV_DIR "shaderspv/"

VkShaderModuleCreateInfo load_spirv(const char* filename)
{
    size_t size = 0;
    void* data = SDL_LoadFile(filename, &size);
    
    if (!data)
    {
        fprintf(stderr, "ERROR: Could not load shader file: %s\n", filename);
        return {};
    }

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = (const uint32_t*)data;
    
    return info;
}

void ShaderRegistry_Init()
{
    // Initialize them to invalid, so we know if we've missed any
    ShaderRegistry* sreg = &renderstate.shader_registry;
    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        PipelineShaderSet* set = &sreg->shaders[i];
        memset(set, 0, sizeof(PipelineShaderSet));
        set->pipeline_type = PK_PIPELINE_TYPE_INVALID;
    }

    // Load shaders (NOTE: Current macro requires types have the same name, so make good use of include statements in the glsl to avoid copy pasting shared vertex shaders)
    #define LOAD_GRAPHICS(id, name_str) \
        sreg->shaders[id].pipeline_type = PK_PIPELINE_TYPE_GRAPHICS; \
        sreg->shaders[id].graphics.vertex_shader = load_spirv(SHADER_SPIRV_DIR name_str ".vert.spv"); \
        sreg->shaders[id].graphics.fragment_shader = load_spirv(SHADER_SPIRV_DIR name_str ".frag.spv");

    LOAD_GRAPHICS(SHADER_UNLIT, "unlit");
    
    // Finally, make sure none are invalid
    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        PipelineShaderSet* set = &sreg->shaders[i];
        SDL_assert(set->pipeline_type != PK_PIPELINE_TYPE_INVALID);
        if (set->pipeline_type == PK_PIPELINE_TYPE_COMPUTE)
        {
            SDL_assert(set->compute.compute_shader.pCode);
        }
        else if (set->pipeline_type == PK_PIPELINE_TYPE_GRAPHICS)
        {
            SDL_assert(set->graphics.vertex_shader.pCode);
            SDL_assert(set->graphics.fragment_shader.pCode);
        }
    }
}

void ShaderRegistry_Shutdown()
{
    ShaderRegistry* sreg = &renderstate.shader_registry;
    for (uint32_t i = 0; i < SHADER_COUNT; ++i)
    {
        PipelineShaderSet* set = &sreg->shaders[i];

        // Have to cast to remove const qualifier
        uint32_t* code;
        if (set->pipeline_type == PK_PIPELINE_TYPE_COMPUTE)
        {
            code = (uint32_t*)set->compute.compute_shader.pCode;   if (code) SDL_free(code);
        }
        else if (set->pipeline_type == PK_PIPELINE_TYPE_GRAPHICS)
        {
            code = (uint32_t*)set->graphics.vertex_shader.pCode;   if (code) SDL_free(code);
            code = (uint32_t*)set->graphics.fragment_shader.pCode; if (code) SDL_free(code);
        }
    }
}
