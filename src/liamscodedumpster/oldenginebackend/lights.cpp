#include "engine.h"

static float
light_range(glm::vec4 color_and_intensity)
{
    // Get range based on attenutation (standard inverse square law at the moment)
    const float minimum_brightness = 0.01;

    // TODO: May update this and the shader to use a different quadratic for attenuation
    
    // NOTE: Solve for radius in intensity at radius = minumum brightness
    // intensity at radius = intensity * attenuation
    // attenuation = 1 / (radius * radius)
    // minumum brightness = intensity / (radius * radius)
    // radius > 0 => radius = sqrt(minumum brightness * intensity)

    // NOTE: This is accurate, HOWEVER if need be could just use color_and_intensity.w on it's own for performance.
    float intensity = glm::length(glm::vec3(color_and_intensity)) * color_and_intensity.w;
    float radius = sqrtf(minimum_brightness * intensity);
    return radius;
}

PointLight
make_point_light(glm::vec3 pos, glm::vec4 color_and_intensity)
{
    PointLight light = {
        .pos_and_radius = glm::vec4(pos, 0.0f),
        .color_and_intensity = color_and_intensity
    };

    float radius = light_range(color_and_intensity);

    light.pos_and_radius.w = radius;

    return light;
}

SpotLight
make_spot_light(glm::vec3 pos, glm::vec4 color_and_intensity, glm::vec4 direction_and_cone_cutoff)
{
    SpotLight light = {
        .pos_and_radius = glm::vec4(pos, 0.0f),
        .color_and_intensity = color_and_intensity,
        .direction_and_cone_cutoff = direction_and_cone_cutoff
    };

    float radius = light_range(color_and_intensity);

    light.pos_and_radius.w = radius;

    return light;
}

void copy_lights_to_gpu(VulkanEngine* engine, FrameData* current_frame)
{
    // Assert that the lights buffer is indeed a mapped portion of memory
    assert(current_frame->lights_buffer.info.pMappedData);

    u64 upload_size = sizeof(LightBufferHeader) +
        sizeof(PointLight) * engine->scene.point_lights_size;
    
    // Check if the fixed sized GPU buffer can contain this much data
    if (upload_size > LIGHT_BUFFER_SIZE)
    {
        fprintf(stderr, "ERROR: Light buffer size overran when copying light to gpu. Shutting down program: recompile with higher allowed light counts if need be.\n");
        assert(0);
        exit(0);
    }

    // Copy header to the mapped region of memory
    LightBufferHeader data_header = {};
    data_header.point_light_count = engine->scene.point_lights_size;

    void* header_ptr = current_frame->lights_buffer.info.pMappedData;
    memcpy(header_ptr, &data_header, sizeof(LightBufferHeader));

    if (engine->scene.point_lights_size == 0)
        return;

    // Copy point lights
    void* point_lights_mapped_ptr = (u8*)header_ptr + sizeof(LightBufferHeader);
    memcpy(point_lights_mapped_ptr, engine->scene.point_lights, engine->scene.point_lights_size * sizeof(PointLight));

#if 0
    // DEBUG: Display the lights in the mapped region:
    for (u32 i = 0; i < engine->scene.point_lights_size; ++i)
    {
        PointLight* light = &((PointLight*)point_lights_mapped_ptr)[i];
        VERBOSE_LOG("light%d: (%f %f %f %f) (%f %f %f %f)\n", i,
            light->pos_and_radius.x, light->pos_and_radius.y, light->pos_and_radius.z, light->pos_and_radius.w,
            light->color_and_intensity.x, light->color_and_intensity.y, light->color_and_intensity.z, light->color_and_intensity.w
        );
    }
#endif
}
