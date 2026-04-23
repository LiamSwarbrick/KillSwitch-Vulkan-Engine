#pragma once

#include "imgui.h"
#include "renderer/renderer.h"   // CameraInfo
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace DebugUI
{
    struct FreeCamState
    {
        glm::vec3 pos     = {  0.0f,  0.0f,  3.0f };
        float     yaw     = -90.0f;
        float     pitch   =   0.0f;
        glm::vec3 forward = {  0.0f,  0.0f, -1.0f };
    };

    // Update free-fly camera and return a CameraInfo ready for Renderer_DrawFrame.
    // Input is hardcoded: hold RMB to look, WASD to fly, E/Q up/down, Shift to sprint.
    // Uses ImGui IO directly — no dependency on core/input.h.
    inline CameraInfo FreeCam_Update(FreeCamState& cam, float dt)
    {
        static constexpr float MOUSE_SENSITIVITY = 0.20f;
        static constexpr float MOVE_SPEED        = 5.0f;
        static constexpr float SPRINT_MULT       = 4.0f;

        ImGuiIO& io = ImGui::GetIO();

        // Rotate: hold right mouse button to look around
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            cam.yaw   += io.MouseDelta.x * MOUSE_SENSITIVITY;
            cam.pitch -= io.MouseDelta.y * MOUSE_SENSITIVITY;
        }

        if (cam.pitch >  89.0f) cam.pitch =  89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        // Compute direction vectors
        glm::vec3 fwd;
        fwd.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        fwd.y = sinf(glm::radians(cam.pitch));
        fwd.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        fwd = glm::normalize(fwd);
        cam.forward = fwd;

        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);

        // Move: WASD + E/Q, Shift to sprint
        float speed = MOVE_SPEED * dt;
        if (io.KeyShift) speed *= SPRINT_MULT;

        if (ImGui::IsKeyDown(ImGuiKey_W)) cam.pos += fwd   * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) cam.pos -= fwd   * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) cam.pos -= right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) cam.pos += right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) cam.pos += up    * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) cam.pos -= up    * speed;

        return CameraInfo{
            .view             = glm::lookAt(cam.pos, cam.pos + fwd, up),
            .position         = cam.pos,
            .lense_distortion = 0.0f
        };
    }

} // namespace DebugUI
