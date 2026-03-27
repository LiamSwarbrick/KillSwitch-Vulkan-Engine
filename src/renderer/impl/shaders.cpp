#include "shaders.h"
#include "internal_state.h"

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
