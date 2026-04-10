#ifndef SHADERSRC_SHARED_MATERIAL_READ_GLSL
#define SHADERSRC_SHARED_MATERIAL_READ_GLSL

void sample_material(
    vec2 uv,
    out MaterialData mat,
    out vec4 base_color)
{
    MaterialBuffer mb = MaterialBuffer(push.dc.material_ptr);
    mat = mb.materials[push.dc.material_idx];

    base_color = mat.base_color;

    // Bindless Sampling
    if (mat.texture_idx_basecolor != UINT32_MAX)  // UINT32_MAX for 'no texture'
    {
        // As per GL_EXT_nonuniform_qualifier
        // "nonuniformEXT()" is required when indexing a descriptor array with a dynamic variable
        base_color *= texture(sampler2D(
            global_textures[nonuniformEXT(mat.texture_idx_basecolor)],
            global_samplers[mat.sampler_idx]),
            uv
        );
    }
}

#endif  // SHADERSRC_SHARED_MATERIAL_READ_GLSL
