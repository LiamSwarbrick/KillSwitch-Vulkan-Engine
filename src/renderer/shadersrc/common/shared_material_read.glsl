#ifndef SHADERSRC_SHARED_MATERIAL_READ_GLSL
#define SHADERSRC_SHARED_MATERIAL_READ_GLSL

void sample_material_basic(
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
