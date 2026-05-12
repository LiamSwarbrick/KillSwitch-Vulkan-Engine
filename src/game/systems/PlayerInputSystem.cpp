#include "PlayerInputSystem.h"

#include "core/input.h"
#include "game/ingame_cam.h"

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"


void PlayerInputSystem::Update(float dt) const
{
    auto view = ecs->GetView<C_PlayerInfo, C_MovementInput, C_CombatInput, C_CombatInfo, C_WeaponSocket>();
    view.ForEach([&](EntityID entity, C_PlayerInfo& playerInfo, C_MovementInput& moveInput, C_CombatInput& combatInput, C_CombatInfo& combatInfo, C_WeaponSocket& weaponSocket)
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
        input.reload = Input_IsActionJustPressed(ACTION_RELOAD);

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

        C_WeaponRanged* weapon = nullptr;
        bool hasWeapon = ecs->Has<C_WeaponRanged>(weaponSocket.weapon_entity);
        if(hasWeapon)
            weapon = ecs->GetComponentPtr<C_WeaponRanged>(weaponSocket.weapon_entity);


        // UPDATE THE STATE
        // If in any state, we're firing, change to firing
        if (combatInfo.isDead)
            playerInfo.state = playerInfo.Dead;
        else if (combatInfo.isStaggered)
            playerInfo.state = playerInfo.Staggered;
        else if (combatInfo.isFiring)
            playerInfo.state = playerInfo.Firing;
        else if (combatInfo.isAttacking)
            playerInfo.state = playerInfo.Attacking;

        // State machine update
        switch (playerInfo.state)
        {
        case playerInfo.Free:
            if (input.aim)
            {
                playerInfo.state = playerInfo.Aiming;
            }
            else if (input.attack)
            {
                // Change to attack if the attack timer is done
                if(combatInfo.attackTimer < 0.0f)
                    playerInfo.state = playerInfo.Attacking;
            }
            else if (input.reload)
            {
                if (hasWeapon 
                    && (weapon->currentBullets < weapon->maxBullets)
                    && (weapon->reloadableBullets > 0))
                {
                    playerInfo.state = playerInfo.Reloading;
                    playerInfo.reloadTimer = weapon->reloadTime;
                }
            }
            break;
        case playerInfo.Attacking:
            if (combatInfo.attackTimer < 0.0f)
            {
                // Change to aiming if we're aiming
                if (input.aim)
                {
                    playerInfo.state = playerInfo.Aiming;
                }
                else if (input.reload)
                {
                    if (hasWeapon 
                        && (weapon->currentBullets < weapon->maxBullets)
                        && (weapon->reloadableBullets > 0))
                    {
                        playerInfo.state = playerInfo.Reloading;
                        playerInfo.reloadTimer = weapon->reloadTime;
                    }
                }
                // if we're not doing attack input, change
                else if (!input.attack)
                {
                    playerInfo.state = playerInfo.Free;
                }
            }
            break;
        case playerInfo.Aiming:
            if (input.aim)
            {
                playerInfo.state = playerInfo.Aiming;
            }
            else if (input.attack)
            {
                playerInfo.state = playerInfo.Attacking;
            }
            else if (input.reload)
            {
                if (hasWeapon 
                    && (weapon->currentBullets < weapon->maxBullets)
                    && (weapon->reloadableBullets > 0))
                {
                    playerInfo.state = playerInfo.Reloading;
                    playerInfo.reloadTimer = weapon->reloadTime;
                }
            }
            else
            {
                playerInfo.state = playerInfo.Free;
            }
            break;
        case playerInfo.Firing:
            if (combatInfo.attackTimer < 0.0f)
            {
                // Change to aiming if we're aiming
                if (input.aim)
                {
                    playerInfo.state = playerInfo.Aiming;
                }
                // if we're not doing attack input, change
                else if (input.attack)
                {
                    playerInfo.state = playerInfo.Attacking;
                }
                else if (input.reload)
                {
                    if (hasWeapon 
                        && (weapon->currentBullets < weapon->maxBullets)
                        && (weapon->reloadableBullets > 0))
                    {
                        playerInfo.state = playerInfo.Reloading;
                        playerInfo.reloadTimer = weapon->reloadTime;
                    }
                }
            }
            break;
        case playerInfo.Reloading:
            playerInfo.reloadTimer -= dt;
            playerInfo.isReloading = true;
            combatInfo.isReloading = true;
            if (input.attack)
            {
                // Cancel the reload if melee (if we're inputting aim we will still trigger melee
                playerInfo.state = playerInfo.Attacking;
                playerInfo.reloadTimer = 0.0f;
                playerInfo.isReloading = false;
                combatInfo.isReloading = false;
            }
            if (playerInfo.reloadTimer < 0.0f)
            {
                // RELOAD THE WEAPON 
                weapon->currentBullets = weapon->reloadableBullets;
                weapon->reloadableBullets = 0;
                playerInfo.isReloading = false;
                combatInfo.isReloading = false;

                // Change to aiming if we're aiming
                if (input.aim)
                {
                    playerInfo.state = playerInfo.Aiming;
                }
                // if we're not doing attack input, change
                else if (input.attack)
                {
                    playerInfo.state = playerInfo.Attacking;
                }
                else
                {
                    playerInfo.state = playerInfo.Free;
                }
            }
            break;
        case playerInfo.Staggered:
            if (combatInfo.staggeredTimer < 0.0f)
            {
                playerInfo.state = playerInfo.Free;
            }
            break;
        case playerInfo.Dead:
            // Don't change state
            break;
        default:
            break;
        }

        moveInput.desiredDir = desiredDir;
        moveInput.moveAmount = moveAmount;
        moveInput.wantsRun = input.run;
        moveInput.wantsCrouch = input.crouch;
        moveInput.wantsJump = input.jump;

        // TODO: instead of wantsAim or wantsReload, we should modify the speed ourselves here (in moveAmount)
        moveInput.wantsAim = input.aim;
        moveInput.wantsReload = input.reload;

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
        if (playerInfo.state == playerInfo.Free)
        {
            // do not allow ranged, Aiming has to come first
            combatInput.wantsRanged = false;
        }
        else if (playerInfo.state == playerInfo.Attacking)
        {
            // do not allow to move, or allow less move when attacking
            moveInput.moveAmount = std::min(moveAmount, 0.0f);
            moveInput.wantsJump = false;
            moveInput.wantsRun = false;

            // Do not let the player shoot while attacking (combat system would deny it anyways)
            combatInput.wantsAim = false;
            combatInput.wantsRanged = false;
        }
        else if (playerInfo.state == playerInfo.Aiming)
        {
            // leave it be (so wantsRanged can be true)
        }
        else if (playerInfo.state == playerInfo.Firing)
        {
            // Do not let the player shoot while attacking (combat system would deny it anyways)
            combatInput.wantsMelee = false;
            combatInput.wantsAim = false;
            combatInput.wantsRanged = false;
        }
        else if (playerInfo.state == playerInfo.Reloading)
        {
            playerInfo.isReloading = true;
            moveInput.moveAmount = std::min(moveAmount, 0.1f);

            combatInput.wantsRanged = false;

            // if we wanted to not allow some sort of movement

        }
        else if (playerInfo.state == playerInfo.Staggered || playerInfo.state == playerInfo.Dead)
        {
            // cancel all input
            moveInput.moveAmount = 0.0f;
            moveInput.wantsRun = false;
            moveInput.wantsCrouch = false;
            moveInput.wantsJump = false;
            moveInput.wantsAim = false;
            moveInput.wantsReload = false;
            moveInput.aimDir = cameraForward;
            combatInput.aimDir = cameraForward;
            combatInput.wantsMelee = false;
            combatInput.wantsAim = false;
            combatInput.wantsRanged = false;

            playerInfo.isReloading = false;
            playerInfo.isAiming = false;
        }



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
