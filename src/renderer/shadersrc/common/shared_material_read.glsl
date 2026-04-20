#ifndef SHADERSRC_SHARED_MATERIAL_READ_GLSL
#define SHADERSRC_SHARED_MATERIAL_READ_GLSL

#extension GL_EXT_samplerless_texture_functions : require

vec2 texture2d_size(uint32_t texture_idx)
{
    return textureSize(global_textures[nonuniformEXT(texture_idx)], 0);
}

vec4 sample_texture2d_with_fallback(vec2 uv, uint32_t texture_idx, uint32_t sampler_idx, vec4 fallback_value)
{
    // Bindless Sampling
    if (texture_idx != UINT32_MAX)  // UINT32_MAX for 'no texture'
    {
        // As per GL_EXT_nonuniform_qualifier
        // "nonuniformEXT()" is required when indexing a descriptor array with a dynamic variable
        return texture(sampler2D(
            global_textures[nonuniformEXT(texture_idx)],
            global_samplers[sampler_idx]),
            uv
        );
    }
    else
    {
        return fallback_value;
    }
}

void sample_material_basic(
    vec2 uv,
    out MaterialData mat,
    out vec4 base_color)
{
    // Intended for when the only thing the shader needs a material for is to read the base color texture
    MaterialBuffer mb = MaterialBuffer(push.dc.material_ptr);
    mat = mb.materials[push.dc.material_idx];
    base_color = sample_texture2d_with_fallback(uv, mat.texture_idx_basecolor, mat.sampler_idx, mat.base_color);
}


float process_alpha(float alpha, float cutoff)
{
    if (CURRENT_BLEND_MODE == BLEND_MODE_MASKED)
    {
        // if (MSAA_SAMPLE_COUNT > 1)
        // {
        //     // Sharpen for Alpha to Coverage (source: https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f)
        //     alpha = (alpha - cutoff) / max(fwidth(alpha), 0.0001) + 0.5;

        //     // TODO: Mipmap adjustment for far folliage to look nice with A2C.
        //     // Unnecessary for now, bcuz we are doing close quarters stuff.
        // }

        if (alpha < cutoff)
        {
            discard;
        }
    }

    return alpha;
}

#endif  // SHADERSRC_SHARED_MATERIAL_READ_GLSL
