#ifndef SHADERSRC_SHARED_LIGHTING_H
#define SHADERSRC_SHARED_LIGHTING_H

#include "shared_types.glsl"

#define MAX_POINTLIGHTS 500
#define MAX_SPOTLIGHTS  500
struct PointLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
};

struct SpotLight
{
    vec4 pos_and_radius;
    vec4 color_and_intensity;
    vec3 direction;
    float inner_cone_angle;
    float outer_cone_angle;
    // TODO: For future shadow map cache, add a dirty bit for if it has moved
};

struct LightsData
{
    uint32_t num_point_lights;
    uint32_t num_spot_lights;
    uint32_t _padding[2];

    PointLight point_lights[MAX_POINTLIGHTS];
    SpotLight  spot_lights[MAX_SPOTLIGHTS];
};

#ifndef IS_GLSL
static
#endif
float get_attenuation(float distance_to_light)
{
    // TODO: Replace with https://lisyarus.github.io/blog/posts/point-light-attenuation.html
    #ifdef IS_GLSL
        return 1.0 / max(distance_to_light*distance_to_light, 1.0);
    #else
        distance_to_light *= distance_to_light;
        if (distance_to_light < 1.0f)
        {
            return 1.0f;
        }
        else
        {
            return 1.0 / distance_to_light;
        }
    #endif
}

#ifndef IS_GLSL

    #include "glm/glm.hpp"

    // Function to get max perceivable distance of point and spot light (based on attenuation)
    static float get_light_radius(glm::vec3 color, float intensity)
    {
        // TODO: https://lisyarus.github.io/blog/posts/point-light-attenuation.html
        // And replace get_attenutation with that one as well
        #warning TODO: IMPLEMENT ATTENTUATION
        return 100.0;
    }

#endif

#endif  // SHADERSRC_SHARED_LIGHTING_H
