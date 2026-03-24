#version 450

layout (location = 0) in float v_area;
layout (location = 1) in vec3 v_barycentric;

layout (location = 0) out vec4 out_color;

void main()
{
    // NOTE: Subpixel triangles 

    // Density is inverse of area
    // d = 1.0 means 1 triangle per pixel
    // d > 1.0 means subpixel triangles
    float density = 1.0 / max(v_area, 1e-4);

    vec3 color;

    const vec3 low_density_color = vec3(0.13, 0.7, 0.50);
    const vec3 med_density_color = vec3(0.9);
    const vec3 high_density_color = vec3(0.4, 0.0, 0.9);

    float threshold = 50.0;  // 1/(Triangles per pixel) to output white
    float normalized_density = density * threshold;
    if (normalized_density <= 1.0)
    {
        color = mix(low_density_color, med_density_color, normalized_density);
    }
    else if (normalized_density <= threshold)
    {
        float factor = clamp((normalized_density - 1.0) * 0.1, 0.0, 1.0);
        color = mix(med_density_color, high_density_color, factor);
    }
    else
    {
        // Subpixel density tends towards yellow
        color = vec3(1.0, 0.5 + 0.5 * density, 0.0);
    }

    // Add wireframe

    vec3 unit_width = fwidth(v_barycentric);
    vec3 edge = smoothstep(vec3(0.0), unit_width * 1.25, v_barycentric);
    float edge_factor = min(min(edge.x, edge.y), edge.z);
    color = mix(0.8 * color, color, edge_factor);

    out_color = vec4(color, 1.0);
}
