#ifndef RENDERER_RENDERPASSES_METADATA_PASS_H
#define RENDERER_RENDERPASSES_METADATA_PASS_H

#include "../framegraph.h"
#include "../pipeline_keying.h"
#include "glm/gtc/matrix_transform.hpp"

// TODO: Add a renderview cache (ideally just big preallocated arrays)
// OR, just gather all renderables, but in each pass, skip over the irrelevant ones.

// NOTE(Liam): Now supporting multiple passes of the same type in the framegraph.
//             Let me know if you find any old comments that indicate otherwise.

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


static
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

void SwapchainPass_Execute(VkCommandBuffer cmd, uint32_t pass_idx);
void DepthPrepass_Execute(VkCommandBuffer cmd,  uint32_t pass_idx);
void ForwardOpaque_Execute(VkCommandBuffer cmd, uint32_t pass_idx);

#endif  // RENDERER_RENDERPASSES_METADATA_PASS_H
