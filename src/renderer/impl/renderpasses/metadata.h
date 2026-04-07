#ifndef RENDERER_RENDERPASSES_METADATA_PASS_H
#define RENDERER_RENDERPASSES_METADATA_PASS_H

#include "../framegraph.h"
#include "../pipeline_keying.h"
#include "glm/gtc/matrix_transform.hpp"

// TODO: Add a renderview cache (ideally just big preallocated arrays)
// OR, just gather all renderables, but in each pass, skip over the irrelevant ones.

typedef enum
{
    PASS_TYPE_SWAPCHAIN_PASS,
    PASS_TYPE_DEPTH_PREPASS,
    PASS_TYPE_FORWARD_OPAQUE,

    PASS_TYPE_COUNT,
    PASS_TYPE_INVALID
}
PassType;
static_assert(PASS_TYPE_COUNT <= MAX_PASSES, "Must increase MAX_PASSES in framegraph.h to store these.");
static_assert(PASS_TYPE_COUNT < 1 << PKEY_NUM_BITS_PASS_TYPE,
    "More passes than pipeline key can index. Must give more bits by increasing PKEY_NUM_BITS_PASS_TYPE in pipeline_hashing.h."
);

static
glm::mat4 MakeProjectionMatrix(float fov_y_radians, float aspect, float near, float far)
{
    glm::mat4 proj = glm::perspective(fov_y_radians, aspect, near, far);
    
    // GLM is OpenGL-style by default:
    // - Y is flipped
    // - Z is -1..1 instead of 0..1
    proj[1][1] *= -1.0f;

    return proj;
}

void SwapchainPass_Execute(VkCommandBuffer cmd, RenderPassDesc* desc);
void DepthPrepass_Execute(VkCommandBuffer cmd,  RenderPassDesc* desc);
void ForwardOpaque_Execute(VkCommandBuffer cmd, RenderPassDesc* desc);

#endif  // RENDERER_RENDERPASSES_METADATA_PASS_H
