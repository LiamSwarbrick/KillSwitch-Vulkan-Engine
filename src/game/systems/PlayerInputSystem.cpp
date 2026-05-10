#include "PlayerInputSystem.h"

#include "core/input.h"
#include "game/ingame_cam.h"

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"


void PlayerInputSystem::Update(float dt) const
{
    auto view = ecs->GetView<C_PlayerInfo, C_MovementInput, C_CombatInput, C_CombatInfo>();
    view.ForEach([&](EntityID entity, C_PlayerInfo& playerInfo, C_MovementInput& moveInput, C_CombatInput& combatInput, C_CombatInfo& combatInfo)
    {
        // Might be dumb and redundant to read from Player Input, if we had an input manager then we would read from it instead of having that.
        // READ FROM: Raw input from the manager, CombatInfo for attack timers
        // WRITE TO: PlayerInfo, MovementInput, CombatInput
        C_PlayerInput input;
        input.move_forward = Input_IsActionPressed(ACTION_MOVE_FORWARD);
        input.forward = Input_GetActionValue(ACTION_MOVE_FORWARD);

        input.move_backward = Input_IsActionPressed(ACTION_MOVE_BACKWARD);
        input.backward = Input_GetActionValue(ACTION_MOVE_BACKWARD);

        input.move_left = Input_IsActionPressed(ACTION_MOVE_LEFT);
        input.left = Input_GetActionValue(ACTION_MOVE_LEFT);

        input.move_right = Input_IsActionPressed(ACTION_MOVE_RIGHT);
        input.right = Input_GetActionValue(ACTION_MOVE_RIGHT);

        input.jump = Input_IsActionJustPressed(ACTION_JUMP);
        input.crouch = Input_IsActionPressed(ACTION_CROUCH);
        input.run = Input_IsActionPressed(ACTION_SPRINT);
        input.aim = Input_IsActionPressed(ACTION_AIM);
        input.attack = Input_IsActionJustPressed(ACTION_ATTACK);

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
        bool isMoving = moveAmount > 0.0f;



        // UPDATE THE STATE
        // If in any state, we're firing, change to firing
        if (combatInfo.isFiring)
            playerInfo.state = playerInfo.Firing;
        else if (combatInfo.isAttacking)
            playerInfo.state = playerInfo.Attacking;
        else if (combatInfo.isStaggered)
            playerInfo.state = playerInfo.Staggered;

        // If in any state we're attacking, 
        switch (playerInfo.state)
        {
        case playerInfo.Free:
            break;
        case playerInfo.Attacking:
            break;
        case playerInfo.Aiming:
            break;
        case playerInfo.Firing:
            break;
        case playerInfo.Reloading:
            break;
        case playerInfo.Staggered:
            break;
        case playerInfo.Dead:
            break;
        default:
            break;
        }

        moveInput.desiredDir = desiredDir;
        moveInput.moveAmount = moveAmount;
        moveInput.wantsRun = input.run;
        moveInput.wantsCrouch = input.crouch;
        moveInput.wantsJump = input.jump;
        moveInput.wantsAim = input.aim;

        moveInput.aimDir = cameraForward;

        combatInput.aimDir = cameraForward;
        combatInput.wantsMelee = input.attack;
        combatInput.wantsAim = input.aim;
        combatInput.wantsRanged = input.aim && input.attack; // assume ranged is aiming + action button == attack

        if (combatInput.wantsRanged)
        {
            SDL_assert(true);
        }

        //// PROCESS THE CURRENT STATE AFTER THE UPDATE
        //if (playerInfo.Attacking)
        //{
        //    // Do not let the player shoot while attacking (combat system would deny it anyways)
        //    combatInput.wantsAim = false;
        //    combatInput.wantsRanged = false;
        //}
        //else if (playerInfo.Firing)
        //{
        //    // Do not let the player shoot while attacking (combat system would deny it anyways)
        //    combatInput.wantsMelee = false;
        //    combatInput.wantsAim = false;
        //    combatInput.wantsRanged = false;
        //}
        //else if (playerInfo.Reloading)
        //{
        //    // Simply do not let it shoot until done
        //    combatInput.wantsAim = false;
        //    combatInput.wantsRanged = false;
        //}



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
