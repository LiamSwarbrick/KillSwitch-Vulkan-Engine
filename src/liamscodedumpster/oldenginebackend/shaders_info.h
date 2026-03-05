#ifndef L_SHADER_INFO_H
#define L_SHADER_INFO_H

#include "vk_layer.h"
#include "glm/glm.hpp"

#include "shared_glsl_defs.h"

/*
TODO NOTE: I'd like to refactor the shader info stuff related to descriptor sets.
I'd also like to specify the info about the render targets formats up here.
But figuring out the easiest way to work with vulkan while having with lots of options and shaders will take time.
*/

// Define shader paths
#define SHADER_SPIRV_DIR "assets/a12/shaders"

#define SPIRV_PATH_SCENE_VERT                       SHADER_SPIRV_DIR"/scene.vert.spv"
#define SPIRV_PATH_FORWARD_LIGHTING_FRAG            SHADER_SPIRV_DIR"/forward_lighting.frag.spv"
#define SPIRV_PATH_DEFERRED_WRITE_GBUFFERS_FRAG     SHADER_SPIRV_DIR"/deferred_write_gbuffers.frag.spv"
#define SPIRV_PATH_DEBUG_VISUALS_FRAG               SHADER_SPIRV_DIR"/debug_visuals.frag.spv"

#define SPIRV_PATH_SHADOW_MAP_VERT                  SHADER_SPIRV_DIR"/shadow_map.vert.spv"
#define SPIRV_PATH_SHADOW_MAP_FRAG                  SHADER_SPIRV_DIR"/shadow_map.frag.spv"

#define SPIRV_PATH_FULLSCREEN_VERT                  SHADER_SPIRV_DIR"/fullscreen.vert.spv"
#define SPIRV_PATH_DEFERRED_LIGHTING_FRAG           SHADER_SPIRV_DIR"/deferred_fullscreen_lighting.frag.spv"
#define SPIRV_PATH_POSTPROCESS_MOSAIC_FRAG          SHADER_SPIRV_DIR"/postprocess_mosaic.frag.spv"
#define SPIRV_PATH_BLOOM_EXTRACT_BRIGHTNESS_FRAG    SHADER_SPIRV_DIR"/bloom_extract_brigthness.frag.spv"
#define SPIRV_PATH_BLOOM_HBLUR_FRAG                 SHADER_SPIRV_DIR"/bloom_hblur.frag.spv"
#define SPIRV_PATH_BLOOM_VBLUR_FRAG                 SHADER_SPIRV_DIR"/bloom_vblur.frag.spv"
#define SPIRV_PATH_BLOOM_APPLY_FRAG                 SHADER_SPIRV_DIR"/bloom_apply_result.frag.spv"

#define SPIRV_PATH_DEBUG_MESH_DENSITY_VERT          SHADER_SPIRV_DIR"/mesh_density.vert.spv"
#define SPIRV_PATH_DEBUG_MESH_DENSITY_GEOM          SHADER_SPIRV_DIR"/mesh_density.geom.spv"
#define SPIRV_PATH_DEBUG_MESH_DENSITY_FRAG          SHADER_SPIRV_DIR"/mesh_density.frag.spv"

// Vertex attribute system:
// There is a fixed set of vertex attributes for each graphics pipeline
// But we can toggle which of those we want.
//
// For meshes, we assume they have all 4.

#define MAX_VERTEX_ATTRIBUTES 4
#define ATTRIB_LOC_POSITION   0  // vec3f
#define ATTRIB_LOC_NORMAL     1  // vec3f
#define ATTRIB_LOC_TEXCOORD   2  // vec2f
#define ATTRIB_LOC_TANGENT    3  // vec4f

#define MAX_VERTEX_BINDINGS      4
#define ATTRIB_BINDING_POSITION  0
#define ATTRIB_BINDING_NORMAL    1
#define ATTRIB_BINDING_TEXCOORD  2
#define ATTRIB_BINDING_TANGENT   3

//
// Descriptor set system:
//

// Scenes:

#define SCENE_DESCRIPTOR_SET_INDEX      0
#define SCENE_DESCRIPTOR_SET_BINDING    0
#define SCENE_DESCRIPTOR_COUNT          1

typedef enum SceneDescriptorSetBindingEnum
{
    SCENE_DESCRIPTOR_SET_BINDING_UNIFORMS = SCENE_DESCRIPTOR_SET_BINDING,

    SCENE_DESCRIPTOR_SET_BINDING_END
}
SceneDescriptorSetBinding;

static_assert(
    SCENE_DESCRIPTOR_COUNT == SCENE_DESCRIPTOR_SET_BINDING_END - SCENE_DESCRIPTOR_SET_BINDING,
    "SceneDescriptorSetBinding enum doesn't match SCENE_DESCRIPTOR_COUNT, update your out of date constants"
);


// Objects:
#define OBJECT_DESCRIPTOR_SET_INDEX 1
#define OBJECT_DESCRIPTOR_SET_BINDING_START 0  // start binding

typedef enum ObjectDescriptorSetBindingEnum
{
    OBJECT_DESCRIPTOR_SET_BINDING_ALBEDO_ALPHA_MAP = OBJECT_DESCRIPTOR_SET_BINDING_START,
    OBJECT_DESCRIPTOR_SET_BINDING_RMA_MAP,
    OBJECT_DESCRIPTOR_SET_BINDING_NORMAL_MAP, 
    OBJECT_DESCRIPTOR_SET_BINDING_EMISSIVE_MAP,

    OBJECT_DESCRIPTOR_SET_BINDING_END
}
ObjectDescriptorSetBinding;

#define OBJECT_DESCRIPTOR_COUNT (OBJECT_DESCRIPTOR_SET_BINDING_END - OBJECT_DESCRIPTOR_SET_BINDING_START)
#define OBJECT_TEXTURE_MAP_COUNT (1 + OBJECT_DESCRIPTOR_SET_BINDING_EMISSIVE_MAP - OBJECT_DESCRIPTOR_SET_BINDING_ALBEDO_ALPHA_MAP)


// Default single pixel textures to use if a material doesn't provide some of them.
const u8 default_albedo_alpha_map[]  = { 255, 255, 255, 255 };  // White, opaque
const u8 default_RMA_map[]           = { 128,   0, 255      };  // Semi-rough, nonmetal, no occlusion
const u8 default_normal_map[]        = { 128, 128, 255      };  // Flat normal (0.5, 0.5, 1.0) NOTE: Z is up in this tangent space
const u8 default_emissive_map[]      = {   0,   0,   0      };  // No emissive

// Formats for each texture map:
const VkFormat default_albedo_alpha_format  = VK_FORMAT_R8G8B8A8_SRGB;  // NOTE: is alpha as sRGB problematic?
const VkFormat default_RMA_format           = VK_FORMAT_R8G8B8_UNORM;
const VkFormat default_normal_format        = VK_FORMAT_R8G8B8_UNORM;
const VkFormat default_emissive_format      = VK_FORMAT_R8G8B8_SRGB;


// Texture system: All meshes will be textured, if a mesh doesn't have one
// It will use a single white pixel as the texture.


// POSTPROCESSING
#define POSTPROCESS_DESCRIPTOR_SET_INDEX 1
#define POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER   0
// #define POSTPROCESS_DESCRIPTOR_SET_BINDING_UNIFORMS  1
#define POSTPROCESS_DESCRIPTOR_COUNT                 1

// BLOOM APPLY (descriptor set layout with two images)
#define BLOOM_APPLY_DESCRIPTOR_SET_INDEX 1
#define BLOOM_APPLY_DESCRIPTOR_SET_BINDING_SCENE_TEXTURE 0
#define BLOOM_APPLY_DESCRIPTOR_SET_BINDING_BLOOM_TEXTURE 1
#define BLOOM_APPLY_DESCRIPTOR_COUNT 2

// DEFERRED SHADING GBUFFERS
#define GBUFFERS_DESCRIPTOR_SET_INDEX 1
typedef enum GBuffersDescriptorSetBindingEnum
{
    GBUFFERS_DESCRIPTOR_SET_BINDING_ALBEDO_ROUGHNESS=0,
    GBUFFERS_DESCRIPTOR_SET_BINDING_NORMAL_METALNESS,
    GBUFFERS_DESCRIPTOR_SET_BINDING_EMISSIVE_AO,
    GBUFFERS_DESCRIPTOR_SET_BINDING_DEPTH,

    RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT
}
GBuffersDescriptorSetBinding;
// NOTE: Depth is seperate because it's not a colour texture (annoying that it's like this at the moment)
#define  RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT (RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT-1)


// LIGHTS
#define LIGHTS_DESCRIPTOR_SET_INDEX 2
#define LIGHTS_DESCRIPTOR_SET_BINDING 0

// SHADOW MAPS
#define SHADOW_MAPS_DESCRIPTOR_SET_INDEX 3
#define SHADOW_MAPS_DESCRIPTOR_SET_BINDING 0

#endif  // L_SHADER_INFO_H
