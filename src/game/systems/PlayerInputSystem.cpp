#include "PlayerInputSystem.h"

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

#include "game/ingame_cam.h"

void PlayerInputSystem::Update(float dt) const
{
    auto view = ecs->GetView<C_PlayerInput, C_MovementInput, C_CombatInput>();
    view.ForEach([&](EntityID entity, C_PlayerInput& input, C_MovementInput& moveInput, C_CombatInput& combatInput)
    {
        // Might be dumb and redundant to read from Player Input, if we had an input manager then we would read from it instead of having that.
        // READ FROM: PlayerInput (for now as a refactor, to read directly from the "input manager")
        // WRITE TO: MovementInput, CombatInput

        // Flatten the camera forward vector so physicsCharacter movement stays horizontal.
        auto flattenDirection = [](const glm::vec3& direction)
        {
            glm::vec3 flattened = glm::vec3(direction.x, 0.0f, direction.z);
            float len = glm::length(flattened);
            if (len <= 0.0001f)
                return glm::vec3(0.0f, 0.0f, -1.0f);

            return flattened / len;
        };

        // moving forward is now relative to the camera
        glm::vec3 cameraForward = InGameCam_GetMovementForward();

        glm::vec3 flattenedForward = -flattenDirection(cameraForward);
        glm::vec3 flattenedRight = glm::normalize(glm::cross(flattenedForward, glm::vec3(0.0f, 1.0f, 0.0f)));

        glm::vec3 horizontalRawMove(0.0f); // we will not operate with 
        float moveAmount = 0.0f;

        // SHOULD ONLY USE THIS IF WE'RE MOVING
        glm::vec3 desiredDir(0.0f);

        if (input.move_forward)
            horizontalRawMove -= flattenedForward * input.forward;
        if (input.move_backward)
            horizontalRawMove += flattenedForward * input.backward;
        if (input.move_left)
            horizontalRawMove += flattenedRight * input.left;
        if (input.move_right)
            horizontalRawMove -= flattenedRight * input.right;

        desiredDir = glm::normalize(horizontalRawMove);
        moveAmount = std::min(glm::length(horizontalRawMove), 1.0f);

        // Write everything to C_MovementInput and C_CombatInput
        moveInput.desiredDir = desiredDir;
        moveInput.moveAmount = moveAmount;
        moveInput.wantsRun = input.run;
        moveInput.wantsCrouch = input.crouch;
        moveInput.wantsJump = input.jump;
        moveInput.wantsAim = input.aim;
        moveInput.aimDir = -flattenedForward;

        combatInput.aimDir = cameraForward;
        combatInput.wantsMelee = input.attack;
        combatInput.wantsRanged = input.aim && input.attack; // assume ranged is aiming + action button == attack


        if (ecs->Has<C_WeaponSocket>(entity))
        {
            auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
            bool hasWeapon = (socket.weapon_entity != NULL_ENTITY) && ecs->IsEntityValid(socket.weapon_entity);

            if (hasWeapon)
            {
                socket.equipped = true;//input.aim;
            }
        }
        

    });
}
