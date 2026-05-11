#include "AnimationSystem.h"

#include "core/animation.h"
#include "core/utils/math_utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

void AnimationSystem::Update(float dt) const
{
    // Update player animations
    UpdatePlayer(dt);

    // Update zombie animations
}

void AnimationSystem::UpdatePlayer(float dt) const
{
    auto view = ecs->GetView<C_Transform, C_MovementInput, C_MovementInfo, C_CombatInput, C_AnimatedMesh>();
    view.ForEach([&](EntityID entity, C_Transform& transform, C_MovementInput& moveInput, C_MovementInfo& moveInfo, C_CombatInput& combatInput, C_AnimatedMesh& animatedMesh)
    {
        animatedMesh.playbackSpeed = 1.0f;
        bool hasWeapon = false;

        // NEEDS REWORK TO THIS
        if (ecs->Has<C_WeaponSocket>(entity))
        {
            auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
            hasWeapon = (socket.weapon_entity != NULL_ENTITY) && ecs->IsEntityValid(socket.weapon_entity) && socket.equipped;
        }

        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(transform.matrix, scale, rotation, translation, skew, perspective);

        // Small tweak to update the rotation
        if (combatInput.wantsAim || moveInfo.isMoving)
        {
            // update rotation 
            // WE CAN EITHER UPDATE THE ROTATION TO THE HORIZONTAL MOVE DIR OR
            // update the rotation to the final controller's velocity (after processing Update), that way we always face where we are moving to instead of input
            // face forward when aiming
            glm::vec3 facingDir;
            if (combatInput.wantsAim)
                facingDir = moveInput.aimDir;
            else
                facingDir = moveInput.desiredDir;

            float yawRad = atan2f(facingDir.x, facingDir.z);

            // Interpolate towards facingDir instead of immediate snap.
            if (moveInput.lastYaw <= 2.0f * M_PI)
            {
                float delta = remainderf(yawRad - moveInput.lastYaw, 2.0f * M_PI);
                float t = 1.0f - expf(-12.0f * dt);
                yawRad = moveInput.lastYaw + delta * t;
                yawRad = remainderf(yawRad, 2.0f * M_PI);
            }
            rotation = glm::angleAxis(yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
            moveInput.lastYaw = yawRad;

            transform.matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

            if (combatInput.wantsAim)
            {
                animatedMesh.aimPitch = -glm::degrees(asinf(facingDir.y));
                //animatedMesh.aimYaw = 0.0f;
            }
        }
        animatedMesh.isAiming = combatInput.wantsAim;

        glm::vec3 movingDir = moveInput.desiredDir;
        glm::vec3 forwardDir = Math::QuatToViewDir(rotation); // Might be the same as moveDir
        struct DirectionalInput
        {
            bool isMovingForward = false;
            bool isMovingBackward = false;
            bool isMovingRight = false;
            bool isMovingLeft = false;
        };
        DirectionalInput dirInput;
        if (glm::dot(movingDir, forwardDir) > 1.0f - 1e-6f)
        {
            dirInput.isMovingForward = true;
        }
        else if (moveInfo.isMoving)
        {
            const float moveThreshold = 0.01f;
            glm::vec3 rightDir = glm::cross(forwardDir, glm::vec3(0.0f, 1.0f, 0.0f));

            float forwardDot = glm::dot(movingDir, forwardDir);
            float rightDot = glm::dot(movingDir, rightDir);

            dirInput.isMovingForward = forwardDot > moveThreshold;
            dirInput.isMovingBackward = forwardDot < -moveThreshold;
            dirInput.isMovingRight = rightDot > moveThreshold;
            dirInput.isMovingLeft = rightDot < -moveThreshold;
        }
        

        bool hasInput =
            moveInput.moveAmount > 0.0f ||
            moveInput.wantsJump ||
            moveInput.wantsCrouch ||
            moveInput.wantsRun ||
            combatInput.wantsAim ||
            combatInput.wantsMelee;

        if (hasInput)
            moveInfo.idleTimer = 0.0f;
        else
            moveInfo.idleTimer += dt;

        const char* prefix = hasWeapon ? "pistol" : "";

        std::string reloadAnimName = "reload";
        int reloadAnimId = GetAnimationIdFromName(animatedMesh, reloadAnimName.c_str());

        if (moveInfo.isReloading && reloadAnimId != -1)
        {
            SDL_Log("curururu");
            if (animatedMesh.upperBodyLayer.currentAnimation != reloadAnimId)
            {
                SDL_Log("curururu");
                PlayUpperBodyAnim(animatedMesh, reloadAnimName.c_str(), 0.2f);
                SetLooping(animatedMesh, animatedMesh.upperBodyLayer, false);
            }
        }
        else
        {
            if (animatedMesh.isUpperLayerActive && animatedMesh.upperBodyLayer.currentAnimation == reloadAnimId)
            {
                StopUpperBodyAnim(animatedMesh, 0.2f);
            }
        }
        if (moveInfo.isJumping || !moveInfo.isGrounded)
        {
            std::string animName = std::string(prefix) + "jump";

            int jumpAnimId = GetAnimationIdFromName(animatedMesh, animName.c_str());

            if (animatedMesh.lowerBodyLayer.currentAnimation != jumpAnimId)
            {
                PlayAnim(animatedMesh, animName.c_str(), 0.10f);
                SetLooping(animatedMesh, animatedMesh.lowerBodyLayer, false);
            }

            animatedMesh.playbackSpeed = 0.6f;
        }
        else if (moveInfo.isMoving)
        {
            std::string animName;

            if (combatInput.wantsAim && hasWeapon)
            {
                if (dirInput.isMovingForward)
                    animName = "pistolwalk";
                else if (dirInput.isMovingBackward)
                    animName = "pistolwalkbackwards";
                else if (dirInput.isMovingLeft)
                {
                    animName = "pistolstrafeleft";
                    animatedMesh.playbackSpeed = 0.75f;
                }
                else if (dirInput.isMovingRight)
                {
                    animName = "pistolstraferight";
                    animatedMesh.playbackSpeed = 0.75f;
                }
                else
                    animName = "pistolwalk";
            }
            else
            {
                if (moveInfo.state == MoveState::Sprint)
                    animName = std::string(prefix) + "run";
                else
                    animName = std::string(prefix) + "walk";
            }

            int moveAnimId = GetAnimationIdFromName(animatedMesh, animName.c_str());

            if (moveAnimId != -1 &&
                animatedMesh.lowerBodyLayer.currentAnimation != moveAnimId)
            {
                PlayAnim(animatedMesh, animName.c_str(), 0.15f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
        else
        {
            std::string animName;

            if (moveInfo.idleTimer > 20.0f)
            {
                animName = std::string(prefix) + "idleafk";
            }
            else
            {
                animName = std::string(prefix) + "idle";
            }

            int idleAnimId =
                GetAnimationIdFromName(animatedMesh, animName.c_str());

            if (animatedMesh.lowerBodyLayer.currentAnimation != idleAnimId)
            {
                PlayAnim(animatedMesh, animName.c_str(), 0.25f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }

        if (ecs->Has<C_WeaponSocket>(entity))
        {
            auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
            hasWeapon = (socket.weapon_entity != NULL_ENTITY) && ecs->IsEntityValid(socket.weapon_entity) && socket.equipped;
        
            if (!hasWeapon)
                return;

            C_Transform& weaponTransform = ecs->GetComponent<C_Transform>(socket.weapon_entity);

            int handJointIndex = -1;

            // find node for hand
            SDL_assert(animatedMesh.asset->skin_count == 1 && "Assuming player has one gltf skin");

            Skin* skin = &animatedMesh.asset->skins[0];
            for (uint32_t i = 0; i < skin->joint_count; i++)
            {
                // Node* node = &animatedMesh.asset->nodes[skin->joint_node_indices[i]];
                Bone* joint = &skin->bones[i];
                if (joint->name && strcmp(joint->name, "mixamorig:RightHand") == 0)
                {
                    handJointIndex = (int)i;
                    break;
                }
            }

            if (handJointIndex != -1)
            {
                glm::mat4 inverseBind =
                    glm::make_mat4(animatedMesh.asset->skins[0].inverse_bind_matrices +
                        handJointIndex * 16);

                glm::mat4 bindMatrix = glm::inverse(inverseBind);

                glm::mat4 animatedMatrix =
                    animatedMesh.joint_matrices[handJointIndex] *
                    bindMatrix;

                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;

                glm::decompose(
                    animatedMatrix,
                    scale,
                    rotation,
                    translation,
                    skew,
                    perspective
                );

                glm::mat4 handMatrix =
                    glm::translate(glm::mat4(1.0f), translation) *
                    glm::mat4_cast(rotation);

                glm::mat4 weaponOffset =
                    glm::translate(glm::mat4(1.0f),
                        glm::vec3(0.07f, 0.12f, 0.061f)) *  // +x is down. +z is left.

                    glm::mat4_cast(glm::quat(glm::vec3(
                        glm::radians(-17.0f),
                        glm::radians(-90.0f),
                        glm::radians(-65.0f)
                    ))) *

                    glm::scale(glm::mat4(1.0f),
                        glm::vec3(1.0f));

                weaponTransform.matrix =
                    transform.matrix *
                    handMatrix *
                    weaponOffset;
            }
        }
    });
}
