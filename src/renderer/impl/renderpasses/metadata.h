#ifndef RENDERER_RENDERPASSES_METADATA_PASS_H
#define RENDERER_RENDERPASSES_METADATA_PASS_H

#include "renderer/renderer.h"
#include "../framegraph.h"
#include "../pipeline_keying.h"
#include "glm/gtc/matrix_transform.hpp"

// NOTE(Liam): Now supporting multiple passes of the same type in the framegraph.


typedef enum
{
    PASS_TYPE_SWAPCHAIN_PASS,
    PASS_TYPE_DEPTH_PREPASS,
    PASS_TYPE_FORWARD_OPAQUE,

    PASS_TYPE_COUNT,
    PASS_TYPE_INVALID
}
PassType;
static_assert(PASS_TYPE_COUNT <= MAX_PASSES,
    "Must increase MAX_PASSES in framegraph.h to store these. Realistically, MAX_PASSES should be substantially higher."
);


glm::mat4 MakeProjectionMatrix(float fov_y_radians, float aspect, float near, float far);
SceneData MakeSceneData(CameraInfo cam, VkExtent2D extents);

void SwapchainPass_Execute(VkCommandBuffer cmd, uint32_t pass_idx);
void DepthPrepass_Execute(VkCommandBuffer cmd,  uint32_t pass_idx);
void ForwardOpaque_Execute(VkCommandBuffer cmd, uint32_t pass_idx);

#endif  // RENDERER_RENDERPASSES_METADATA_PASS_H
