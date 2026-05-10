#include "AnimationSystem.h"

#include "core/animation.h"

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
    auto view = ecs->GetView<C_Transform, C_PlayerInput, C_MovementInput, C_MovementInfo, C_AnimatedMesh>();
    view.ForEach([&](EntityID entity, C_Transform& transform, C_PlayerInput& playerInput, C_MovementInput& moveInput, C_MovementInfo& moveInfo, C_AnimatedMesh& animatedMesh)
    {
        animatedMesh.playbackSpeed = 1.0f;
        bool hasWeapon = false;

        // NEEDS REWORK TO THIS
        /*if (ecs->Has<C_WeaponSocket>(entity))
        {
            auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
            hasWeapon = (socket.weapon_entity != NULL_ENTITY) && ecs->IsEntityValid(socket.weapon_entity);
        }*/

        ecs->GetView<C_Weapon>().ForEach([&](C_Weapon& weapon) // need to know what type of weapon for different animations
            {
                if (weapon.equipped)
                    hasWeapon = true;
            });

        // Small tweak to update the rotation
        if ((playerInput.aim && moveInfo.isGrounded) || moveInfo.isMoving)
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transform.matrix, scale, rotation, translation, skew, perspective);

            // update rotation 
            // WE CAN EITHER UPDATE THE ROTATION TO THE HORIZONTAL MOVE DIR OR
            // update the rotation to the final controller's velocity (after processing Update), that way we always face where we are moving to instead of input
            // face forward when aiming


            glm::vec3 facingDir;
            if (playerInput.aim)
                facingDir = moveInput.aimDir;
            else
                facingDir = moveInput.desiredDir;

            float yawDeg = glm::degrees(atan2f(facingDir.x, facingDir.z));
            rotation = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));

            transform.matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

        }

        if (moveInfo.isMoving && moveInfo.isGrounded)
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transform.matrix, scale, rotation, translation, skew, perspective);

            // update rotation 
            // WE CAN EITHER UPDATE THE ROTATION TO THE HORIZONTAL MOVE DIR OR
            // update the rotation to the final controller's velocity (after processing Update), that way we always face where we are moving to instead of input
            // face forward when aiming


            glm::vec3 facingDir;
            if (playerInput.aim)
                facingDir = moveInput.aimDir;
            else
                facingDir = moveInput.desiredDir;

            float yawDeg = glm::degrees(atan2f(facingDir.x, facingDir.z));
            rotation = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));

            transform.matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

        }

        bool hasInput =
            playerInput.move_forward ||
            playerInput.move_backward ||
            playerInput.move_left ||
            playerInput.move_right ||
            playerInput.jump ||
            playerInput.run ||
            playerInput.crouch ||
            playerInput.aim ||
            playerInput.attack;

        if (hasInput)
            moveInfo.idleTimer = 0.0f;
        else
            moveInfo.idleTimer += dt;

        const char* prefix = hasWeapon ? "pistol" : "";

        if (moveInfo.isJumping)
        {
            std::string animName = std::string(prefix) + "jump";

            int jumpAnimId = GetAnimationIdFromName(animatedMesh, animName.c_str());

            if (animatedMesh.lowerBodyLayer.currentAnimation != jumpAnimId)
            {
                PlayAnim(animatedMesh, animName.c_str(), 0.10f);
                SetLooping(animatedMesh, animatedMesh.lowerBodyLayer, false);
            }

            animatedMesh.playbackSpeed = 0.6f;

            float dur = GetAnimationDuration(animatedMesh, jumpAnimId);
            if (animatedMesh.lowerBodyLayer.currentAnimationTime >= dur)
            {
                animatedMesh.lowerBodyLayer.currentAnimationTime = dur;
            }
        }
        else if (moveInfo.isMoving)
        {
            std::string animName;

            if (playerInput.aim && hasWeapon)
            {
                if (playerInput.move_forward && !playerInput.move_backward)
                    animName = "pistolwalk";
                else if (playerInput.move_backward && !playerInput.move_forward)
                    animName = "pistolwalkbackwards";
                else if (playerInput.move_left && !playerInput.move_right)
                    animName = "pistolstrafeleft";
                else if (playerInput.move_right && !playerInput.move_left)
                    animName = "pistolstraferight";
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

        ecs->GetView<C_Weapon, C_Transform>().ForEach([&](C_Weapon& weapon, C_Transform& weaponTransform)
        {
            if (!weapon.equipped)
                return;

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
                // glm::mat4 handMatrix = animatedMesh.joint_matrices[handJointIndex];
                // weaponTransform.matrix = transform.matrix * handMatrix;

                glm::mat4 inverseBind =
                    glm::make_mat4(animatedMesh.asset->skins[0].inverse_bind_matrices +
                                handJointIndex * 16);

                glm::mat4 bindMatrix = glm::inverse(inverseBind);

                glm::mat4 handMatrix =
                    animatedMesh.joint_matrices[handJointIndex] *
                    bindMatrix;

                weaponTransform.matrix =
                    transform.matrix *
                    handMatrix;
            }
        });
    });
}
