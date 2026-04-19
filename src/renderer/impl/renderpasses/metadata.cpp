#include "metadata.h"
#include "internal_state.h"

glm::mat4 MakeProjectionMatrix(float fov_y_radians, float aspect, float near, float far)
{
    // A project matrix that aligns with our internal coordinate system (Vulkan NDC Space).

    glm::mat4 proj = glm::perspective(fov_y_radians, aspect, near, far);
    
    // GLM is OpenGL-style by default:
    // - Y is flipped
    // - Z is -1..1 instead of 0..1
    proj[1][1] *= -1.0f;

    return proj;
}

SceneData MakeSceneData(CameraInfo cam, VkExtent2D extents)
{
    float aspect = (float)extents.width / (float)extents.height;
    glm::mat4 proj = MakeProjectionMatrix(glm::radians(renderstate.settings.fov_y), aspect, 0.1f, 100.0f);
    glm::mat4 view_proj = proj * cam.view;
    glm::uvec2 extents_uvec2 = glm::uvec2(extents.width, extents.height);

    SceneData data = {};
    memcpy(data.view, glm::value_ptr(cam.view), sizeof(glm::mat4));
    memcpy(data.proj, glm::value_ptr(proj), sizeof(glm::mat4));
    memcpy(data.view_proj, glm::value_ptr(view_proj), sizeof(glm::mat4));
    memcpy(data.cam_position, glm::value_ptr(cam.position), sizeof(glm::vec3));

    memcpy(data.rendertarget_size, glm::value_ptr(extents_uvec2), sizeof(glm::uvec2));
    data.time = (float)((double)SDL_GetTicks() / 1000.0);
    data.aspect = aspect;

    return data;
}
