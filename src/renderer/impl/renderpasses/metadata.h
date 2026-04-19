#ifndef RENDERER_RENDERPASSES_METADATA_PASS_H
#define RENDERER_RENDERPASSES_METADATA_PASS_H

#include "renderer/renderer.h"
#include "../framegraph.h"
#include "../pipeline_keying.h"
#include "glm/gtc/matrix_transform.hpp"

// NOTE(Liam): Now supporting multiple passes of the same type in the framegraph.

typedef struct FullscreenPass_UserData
{
    uint32_t shader_id;
    PushConstant_PassHeader push_pass;
}
FullscreenPass_UserData;


glm::mat4 MakeProjectionMatrix(float fov_y_radians, float aspect, float near, float far);
SceneData MakeSceneData(CameraInfo cam, VkExtent2D extents);

void FullscreenPass_Execute(VkCommandBuffer cmd, uint32_t pass_idx);
void DepthPrepass_Execute(VkCommandBuffer cmd,  uint32_t pass_idx);
void ForwardOpaque_Execute(VkCommandBuffer cmd, uint32_t pass_idx);

#endif  // RENDERER_RENDERPASSES_METADATA_PASS_H
