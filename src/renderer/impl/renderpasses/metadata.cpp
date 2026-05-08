#include "metadata.h"
#include "internal_state.h"

glm::mat4 MakeProjectionMatrix(float fov_y_radians, float aspect, float near, float far)
{
    // A project matrix that aligns with our internal coordinate system (Vulkan NDC Space).

    glm::mat4 proj = glm::perspectiveRH_ZO(fov_y_radians, aspect, near, far);
    proj[1][1] *= -1.0f;  // Vulkan Flips Y

    return proj;
}

SceneData MakeSpotLightSceneData(SpotLight spotlight, VkExtent2D extent)
{
    glm::vec3 pos = glm::vec3(spotlight.pos_and_radius[0], spotlight.pos_and_radius[1], spotlight.pos_and_radius[2]);
    glm::vec3 dir = glm::vec3(spotlight.direction[0], spotlight.direction[1], spotlight.direction[2]);

    glm::mat4 light_view = glm::lookAtRH(pos, pos + dir, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = 1.0f;  // Square shadow map
    float near_plane = 0.1f;
    float far_plane = spotlight.pos_and_radius[3];  // Setting radius to be the far plane (do i need some padding?)
    float fov_y = spotlight.outer_cone_angle * 2.0f;  // <- Angle from centre, hence double it
    glm::mat4 light_proj = glm::perspectiveRH_ZO(fov_y, aspect, near_plane, far_plane);
    light_proj[1][1] *= -1;  // Vulkan Flips Y

    glm::mat4 light_view_proj = light_proj * light_view;
    glm::uvec2 extent_uvec2 = glm::uvec2(extent.width, extent.height);


    SceneData data = {};
    memcpy(data.view, glm::value_ptr(light_view), sizeof(glm::mat4));
    memcpy(data.proj, glm::value_ptr(light_proj), sizeof(glm::mat4));
    memcpy(data.view_proj, glm::value_ptr(light_view_proj), sizeof(glm::mat4));

    memcpy(data.cam_position, glm::value_ptr(pos), sizeof(glm::vec3));
    data.time = (float)((double)SDL_GetTicks() / 1000.0);
    data.near_plane = near_plane;
    data.far_plane = far_plane;
    data.aspect = aspect;
    data.lens_distortion = 0.0f;  // Doesn't do anything in shadowmaps btw

    memcpy(data.rendertarget_size, glm::value_ptr(extent_uvec2), sizeof(glm::uvec2));

    return data;
}

SceneData MakeSceneData(CameraInfo cam, VkExtent2D extent)
{
    float aspect = (float)extent.width / (float)extent.height;
    glm::mat4 proj = MakeProjectionMatrix(glm::radians(renderstate.settings.fov_y), aspect, cam.near_plane, cam.far_plane);
    glm::mat4 view_proj = proj * cam.view;
    glm::uvec2 extent_uvec2 = glm::uvec2(extent.width, extent.height);

    // printf("Projection Matrix\n");
    // printf("[ %f %f %f %f ]\n", proj[0][0], proj[1][0], proj[2][0], proj[3][0]);
    // printf("[ %f %f %f %f ]\n", proj[0][1], proj[1][1], proj[2][1], proj[3][1]);
    // printf("[ %f %f %f %f ]\n", proj[0][2], proj[1][2], proj[2][2], proj[3][2]);
    // printf("[ %f %f %f %f ]\n\n", proj[0][3], proj[1][3], proj[2][3], proj[3][3]);

    SceneData data = {};
    memcpy(data.view, glm::value_ptr(cam.view), sizeof(glm::mat4));
    memcpy(data.proj, glm::value_ptr(proj), sizeof(glm::mat4));
    memcpy(data.view_proj, glm::value_ptr(view_proj), sizeof(glm::mat4));

    memcpy(data.cam_position, glm::value_ptr(cam.position), sizeof(glm::vec3));
    data.time = (float)((double)SDL_GetTicks() / 1000.0);

    data.near_plane = cam.near_plane;
    data.far_plane = cam.far_plane;
    data.aspect = aspect;
    data.lens_distortion = cam.lens_distortion;

    memcpy(data.rendertarget_size, glm::value_ptr(extent_uvec2), sizeof(glm::uvec2));
    data.inv_log_far_over_near = 1.0f / log(cam.far_plane / cam.near_plane);

    return data;
}
