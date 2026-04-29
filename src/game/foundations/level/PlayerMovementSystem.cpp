#include "foundations/PlayerMovementSystem.h"
#include "foundations/components.h"
#include "core/animation.h"

void PlayerMovement_Update(ECS* ecs, float dt)
{
    auto view = ecs->GetView<C_PlayerInput, C_CharacterController, C_Transform, C_AnimatedMesh>();
    view.ForEach([&](EntityID e, C_PlayerInput& input, C_CharacterController& controller, C_Transform& transform, C_AnimatedMesh& animatedMesh)
    {
        glm::vec3 moveDir(0.0f);

        if (input.move_forward)  
            moveDir.z -= 1.0f;
        if (input.move_backward) 
            moveDir.z += 1.0f;
        if (input.move_left)     
            moveDir.x -= 1.0f;
        if (input.move_right)    
            moveDir.x += 1.0f;

        bool isMoving = glm::length(moveDir) > 0.0f;
        if (isMoving)
            moveDir = glm::normalize(moveDir);

        controller.velocity = moveDir * controller.move_speed;
        transform.matrix = glm::translate(transform.matrix, controller.velocity * dt);

        if (isMoving) {
            int runAnimId = GetAnimationIdFromName(animatedMesh, "WALK");
            if (animatedMesh.lowerBodyLayer.currentAnimation != runAnimId) {
                PlayAnim(animatedMesh, "WALK", 0.2f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
        else {
            int idleAnimId = GetAnimationIdFromName(animatedMesh, "IDLE");
            if (animatedMesh.lowerBodyLayer.currentAnimation != idleAnimId) {
                PlayAnim(animatedMesh, "IDLE", 0.2f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
    });
}