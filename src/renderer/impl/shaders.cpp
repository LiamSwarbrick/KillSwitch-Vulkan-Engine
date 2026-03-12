#include "shaders.h"
#include "internal_state.h"

uint64_t get_resource_buffer_device_address(uint32_t rid)
{
    if (rid == UINT32_MAX) return 0;
    return renderstate.registry.resources[rid].buffer_gpu_address;
}

TransientBuffer CreateTransientBuffer(uint32_t underlying_resource_id)
{
    FG_Resource* res = &renderstate.registry.resources[underlying_resource_id];
    
    return {
        .rid              = underlying_resource_id,
        .gpu_base_address = res->buffer_gpu_address,
        .mapped_data      = (uint8_t*)res->buffer.mapped_data,
        .current_offset   = 0,
        .total_size       = res->buffer.size
    };
}

void reset_transient_buffer(TransientBuffer* tb)
{
    tb->current_offset = 0;
}

uint64_t push_object(TransientBuffer* tb, ObjectData object)
{
    uint32_t size = sizeof(ObjectData);
    // Align to 64 bytes (Vulkan requirement for many BDA operations)
    uint32_t aligned_offset = (tb->current_offset + 63) & ~63;
    SDL_assert(aligned_offset + size <= tb->total_size);

    // Copy object to the mapped pointer
    memcpy(tb->mapped_data + aligned_offset, &object, size);
    tb->current_offset = aligned_offset + size;
    
    // Return the absolute GPU address for the Push Constant
    return tb->gpu_base_address + aligned_offset;
}

void UpdateGlobalSceneData()
{
    SceneBufferData data = {};
    
    // TODO: Add camera here.
    data.view = glm::mat4(1.0f);
    data.view[3][1] = -0.5f;
    data.proj = glm::mat4(1.0f);
    data.view_proj = data.proj * data.view;
    FG_UploadBufferData(&renderstate.main.staging_objects, 
                        renderstate.rids.global_scene_buffer_rid, 
                        &data, sizeof(SceneBufferData)
    );

    // Object transforms
    reset_transient_buffer(&renderstate.object_transforms);
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
    PushConstants pc = {};
    pc.scene_ptr    = get_resource_buffer_device_address(renderstate.rids.global_scene_buffer_rid);
    pc.material_ptr = get_resource_buffer_device_address(renderstate.rids.material_ssbo_rid);
    pc.vertex_ptr   = get_resource_buffer_device_address(r->mesh_rid);
    
    pc.object_ptr   = r->object_ptr;
    pc.joint_ptr    = r->vertex_type == VERTEX_TYPE_SKINNED ? r->joint_ptr : 0;
    pc.material_idx = r->material_id;

    vkCmdPushConstants(cmd, renderstate.global_pipeline_layout, 
                       VK_SHADER_STAGE_ALL, 
                       0, sizeof(PushConstants), &pc);

    // Draw
    // Since we use Vertex Pulling, we don't bind vertex buffers.
    // We just need the count of vertices in the buffer.
    FG_Resource* mesh_res = &renderstate.registry.resources[r->mesh_rid];
    uint32_t vertex_count = mesh_res->buffer.size / sizeof(Vertex);
    
    vkCmdDraw(cmd, vertex_count, 1, 0, 0);
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
