#include "pipeline_keying.h"
#include "stb_ds.h"             // NOTE: STB_DS_IMPLEMENTATION is defined at the top of renderer.cpp
#include "SDL3/SDL.h"
#include "renderpasses/metadata.h"
#include "../render_types.h"
#include "shaders.h"
#include "internal_state.h"

/*
Intelligent pipeline creation by inspecting input and output attachments
of the renderpass set in the pipeline key in order to set the 
attachments for VkGraphicsPipelineCreateInfo.

IMPORTANT IMPLEMENTATION NOTE:
- A specific renderpass type must be consistent in terms of attachment count and formats from frame to frame.
  because it would fuck with the above mentioned thing (the pipeline is hashed based on pass_id not attachment formats).
  The framegraph is built every frame but this just means the inputs and outputs can change
  The number of inputs/outputs to a specific pass along with their VkFormat's must be hard set.

DONE: Graphics Pipelines creation
TODO: Compute Pipeline creation (wayyy fucking simpler, but not needed compute shaders yet)
*/

void PK_Init(PipelineEntry** pipeline_map_ref)
{
    *pipeline_map_ref = NULL;  // Set to NULL so stb_ds will make a new hash map.

    // Load shaders
    ShaderRegistry_Init();
}

VkPipeline create_graphics_pipeline(PipelineKey key);

VkPipeline PK_GetOrCreatePipeline(PipelineEntry** pipeline_map_ref, PipelineKey key)
{
    ptrdiff_t index = hmgeti(*pipeline_map_ref, key.value);
    if (index >= 0)
    {
        // Found in cache, return premade pipeline
        VkPipeline cached_pipeline = (*pipeline_map_ref)[index].value;
        return cached_pipeline;
    }

    // Not found in hashmap, create new pipeline
    //

    VkPipeline new_pipeline = VK_NULL_HANDLE;
    switch ((PK_PipelineType)key.pipeline_type)
    {
    case PK_PIPELINE_TYPE_COMPUTE:
        // TODO.
        SDL_assert(1 && "TODO: make compute pipeline.");
        break;

    case PK_PIPELINE_TYPE_GRAPHICS:
        new_pipeline = create_graphics_pipeline(key);
        break;
        
    default:
        SDL_assert(0 && "Invalid pipeline type");
    }

    // Add new pipeline to hash map
    hmput(*pipeline_map_ref, key.value, new_pipeline);


    SDL_assert(new_pipeline != VK_NULL_HANDLE);
    return new_pipeline;
}

void PK_Shutdown(PipelineEntry** pipeline_map_ref, VkDevice device)
{
    ShaderRegistry_Shutdown();

    for (uint32_t i = 0; i < hmlenu(*pipeline_map_ref); ++i)
    {
        vkDestroyPipeline(device, (*pipeline_map_ref)[i].value, NULL);
    }

    hmfree(*pipeline_map_ref);  // Frees and sets the pointer to NULL.
}

VkPipeline create_graphics_pipeline(PipelineKey key)
{
    // Fetch the Shader Set from the registry
    PipelineShaderSet* shader_set = &renderstate.shader_registry.shaders[key.shader_id];
    SDL_assert(shader_set->pipeline_type == PK_PIPELINE_TYPE_GRAPHICS);

    // Shader specialization constants
    struct SpecializationData {
        uint32_t vertex_type;
        uint32_t blend_mode;
    } spec_values;

    spec_values.vertex_type = key.vertex_type;
    spec_values.blend_mode = key.blend_mode;

    VkSpecializationMapEntry spec_entries[] = {
        { 0, offsetof(SpecializationData, vertex_type), sizeof(uint32_t) },
        { 1, offsetof(SpecializationData, blend_mode),  sizeof(uint32_t) }
    };

    VkSpecializationInfo spec_info = {
        .mapEntryCount  = sizeof(spec_entries)/sizeof(spec_entries[0]),
        .pMapEntries    = spec_entries,
        .dataSize       = sizeof(SpecializationData),
        .pData          = &spec_values
    };
    
    // Shader stages (with VK_KHR_maintenance5 to skip VkShaderModule)
    const uint32_t max_stages = 2;
    VkPipelineShaderStageCreateInfo stages[max_stages] = {};
    
    const uint32_t num_stages = 2;  // TODO: Update this when adding geometry shader etc...

    // Vertex
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].pName = "main";
    stages[0].pNext = &shader_set->graphics.vertex_shader;
    stages[0].pSpecializationInfo = &spec_info;

    // Fragment
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].pName = "main";
    stages[1].pNext = &shader_set->graphics.fragment_shader;
    stages[1].pSpecializationInfo = &spec_info;

    // Fixed-Function State (Rasterizer)
    VkPipelineRasterizationStateCreateInfo raster_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster_info.polygonMode = (VkPolygonMode)key.polygon_mode;
    raster_info.lineWidth   = 1.0f;
    raster_info.cullMode    = (VkCullModeFlags)key.cull_mode;
    raster_info.frontFace   = (VkFrontFace)key.front_face;

    // Fixed-Function State (Multisampling)
    VkPipelineMultisampleStateCreateInfo multisample_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // Default to 1x
    // TODO: Implement MSAA.
    // FUTURE: Implement MSAA with Specular AA

    // Fixed-Function State (Depth/Stencil)
    VkPipelineDepthStencilStateCreateInfo depth_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_info.depthTestEnable   = (VkBool32)key.depth_test;
    depth_info.depthWriteEnable  = (VkBool32)key.depth_write;
    depth_info.depthCompareOp    = (VkCompareOp)key.depth_op;
    depth_info.stencilTestEnable = key.stencil_mode != 0;

    // Fixed-Function State (Blending)
    VkPipelineColorBlendAttachmentState color_blend = {};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    if (key.blend_mode == BLEND_MODE_BLEND)
    {
        color_blend.blendEnable         = VK_TRUE;
        color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    else if (key.blend_mode == BLEND_MODE_ADDITIVE)
    {
        color_blend.blendEnable         = VK_TRUE;
        color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend.colorBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo blend_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend_info.attachmentCount = 1;
    blend_info.pAttachments    = &color_blend;

    // Find attachment formats from the framegraph using key.pass_type to get pass_id
    RenderPassDesc* pass = &renderstate.framegraph.passes[renderstate.pass_id_from_type[key.pass_type]];
    ResourceRegistry* reg = &renderstate.registry;

    // NOTE: Local array for color_attachment_formats, so make sure to not to refactor things into different scopes
    uint32_t color_attachment_count = 0;
    VkFormat color_attachment_formats[MAX_PASS_RESOURCE_BANDWIDTH] = {};
    uint32_t depth_attachment_rid   = UINT32_MAX;
    uint32_t stencil_attachment_rid = UINT32_MAX;

    for (uint32_t i = 0; i < pass->output_count; ++i)
    {
        PassResourceUsage* usage = &pass->outputs[i];
        if (usage->usage_flags == FG_USAGE_COLOR)
        {
            SDL_assert(usage->rid < reg->resource_count);
            color_attachment_formats[color_attachment_count++] = reg->resources[usage->rid].image.format;
        }
        else if (usage->usage_flags == FG_USAGE_DEPTH)
        {
            SDL_assert(depth_attachment_rid == UINT32_MAX && "Can only have one depth attachment!");
            depth_attachment_rid = usage->rid;
        }
        else if (usage->usage_flags == FG_USAGE_STENCIL)
        {
            SDL_assert(stencil_attachment_rid == UINT32_MAX && "Can only have one stencil attachment!");
            stencil_attachment_rid = usage->rid;
        }
    }

    // Dynamic State:
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyn_info.dynamicStateCount = 2;
    dyn_info.pDynamicStates    = dynamic_states;

    // We provide empty placeholders for state we manage via Vertex Pulling
    VkPipelineVertexInputStateCreateInfo vertex_input = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.vertexAttributeDescriptionCount = 0;
    
    VkPipelineInputAssemblyStateCreateInfo input_asm = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_asm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // FUTURE: This can be part of the pipeline key for things like terrain that may use triangle strips instead.
    input_asm.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessellation = { .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
    // No tesselation shaders at the moment so leaving this empty.

    // Viewport is set to dynamic to avoid remaking pipelines on resize, so this is mostly ignored.
    VkPipelineViewportStateCreateInfo viewport_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    // Dynamic Rendering Info (To avoid needing a VkRenderPass object)
    VkPipelineRenderingCreateInfo rendering_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering_info.colorAttachmentCount = color_attachment_count;
    rendering_info.pColorAttachmentFormats = color_attachment_formats;

    if (depth_attachment_rid != UINT32_MAX)
    {
        rendering_info.depthAttachmentFormat = reg->resources[depth_attachment_rid].image.format;
    }
    else
    {
        rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    }

    if (stencil_attachment_rid != UINT32_MAX)
    {
        rendering_info.stencilAttachmentFormat = reg->resources[stencil_attachment_rid].image.format;
    }
    else
    {
        rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    }

    // Final Creation
    VkGraphicsPipelineCreateInfo pipe_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipe_info.pNext               = &rendering_info;  // <- Dynamic rendering info goes in pNext chain
    pipe_info.stageCount          = num_stages;
    pipe_info.pStages             = stages;
    pipe_info.pVertexInputState   = &vertex_input;
    pipe_info.pInputAssemblyState = &input_asm;
    pipe_info.pTessellationState  = &tessellation;
    pipe_info.pViewportState      = &viewport_state;
    pipe_info.pRasterizationState = &raster_info;
    pipe_info.pMultisampleState   = &multisample_info;
    pipe_info.pDepthStencilState  = &depth_info;
    pipe_info.pColorBlendState    = &blend_info;
    pipe_info.pDynamicState       = &dyn_info;
    pipe_info.layout              = renderstate.global_pipeline_layout;
    

    

    VkPipeline new_pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(renderstate.device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &new_pipeline));
    // TODO: Replace VK_NULL_HANLDE with renderstate.pipeline_cache_vulkan and serialise pipelines on exit.

    return new_pipeline;
}
