#include "PlayerMovementSystem.h"

#include "foundations/components.h"
#include "core/animation.h"


#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"



void PlayerMovementSystem::Update(ECS& ecs, PhysicsManager& physics, EntityID playerID, const glm::vec3& forward, float dt)
{
    // BIG IMPORTANT NOTE(jaime):
    // WE NEED A fookin' class for this. if the physicsCharacter "crouches" we NEED to have a smaller capsule, 
    // and i would appreciate if we had all IShapes stored in advance instead of creating them on the fly
    // (PLEASE ALL CHARACTER SHAPES HAVE TO BE capsules please i beg you, otherwise physics die )

    if (playerID == NULL_ENTITY) return;
    if (!ecs.Has(playerID)) return;

    PhysicsCharacter* physicsCharacter = physics.getCharacter(playerID);
    if (!physicsCharacter) return;

    C_Transform& transform = ecs.GetComponent<C_Transform>(playerID);
    C_PlayerInput& input = ecs.GetComponent<C_PlayerInput>(playerID);
    C_PlayerController& controller = ecs.GetComponent<C_PlayerController>(playerID);

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform.matrix, scale, rotation, translation, skew, perspective);

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
    glm::vec3 cameraForward = -flattenDirection(forward);
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraForward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 horizontalMoveDir(0.0f);
    glm::vec3 horizontalRawMove(0.0f); // we will not operate with 
    float moveAmount = 0.0f;

    // SHOULD ONLY USE THIS IF WE'RE MOVING
    glm::vec3 desiredDir(0.0f);

    if (input.move_forward)
        horizontalRawMove -= cameraForward * input.forward;
    if (input.move_backward)
        horizontalRawMove += cameraForward * input.backward;
    if (input.move_left)
        horizontalRawMove += cameraRight * input.left;
    if (input.move_right)
        horizontalRawMove -= cameraRight * input.right;

    moveAmount = std::min(glm::length(horizontalRawMove), 1.0f);
    bool isMoving = moveAmount > 0.0f;
    if (isMoving)
    {
        horizontalMoveDir = glm::normalize(horizontalRawMove); // Get the direction (normalized)
        
        // update rotation 
        // WE CAN EITHER UPDATE THE ROTATION TO THE HORIZONTAL MOVE DIR OR
        // update the rotation to the final controller's velocity (after processing Update), that way we always face where we are moving to instead of input
        // face forward when aiming
        glm::vec3 facingDir;
        if(input.aim)
            facingDir = flattenDirection(forward);
        else
            facingDir = glm::normalize(glm::vec3(horizontalMoveDir.x, 0.0f, horizontalMoveDir.z));

        float yawDeg = glm::degrees(atan2f(facingDir.x, facingDir.z));
        rotation = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));

        transform.matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

        // Project horizontal velocity to character's slope normal
        glm::vec3 slopeRight = glm::cross(horizontalMoveDir, physicsCharacter->groundNormal);
        glm::vec3 slopeForward = glm::cross(physicsCharacter->groundNormal, slopeRight);

        //if (slopeForward.y > 0.0f) slopeForward.y = 0.0f; // Do this to walk down surfaces correctly.
        //slopeForward.y = 0.0f;

        desiredDir = slopeForward;
    }

    // If we are jumping, we need to check IF we touch the floor
    // We take away time from the jumping cooldown using dt to check next step
    /*if (controller.jumping)
    {
        controller.jumping_cooldown -= dt;
    }*/


    // We only need to check if the jumping cooldown is done
    //if (controller.jumping_cooldown < 0.0f)
    //{
    //    // Raycast to update if we are jumping
    //    // For that, we need the current Character's transform AND shape to find the feet, and then throw a very tiny ray
    //    //IShape* shape = m_physicsManager.getShape(m_currentPlayer);
    //    //ShapeType shapeType = shape->getType();
    //    //if (shapeType != ShapeType::Capsule)
    //    //{
    //    //    SDL_assert(false && "Character collider (IShape) needs to be of ShapeType::Capsule");
    //    //    return;
    //    //}
    //    //CapsuleShape* capsuleShape = static_cast<CapsuleShape*>(shape);
    //    //
    //    //Ray feetRay;
    //    //feetRay.origin = translation; // We will have to add the capsule's halfHeight +(-) radius to get the feet
    //    //feetRay.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    //    //feetRay.maxDistance = capsuleShape->halfHeight + capsuleShape->radius + 0.15f; // add a small distance to the halfheight + radius

    //    //QueryFilterExternal filter;
    //    //filter.bodyToIgnore = m_currentPlayer;
    //    //filter.hasLayerOfQuery = true;
    //    //filter.layerOfQuery = (uint8_t)BodyLayer::MOVING;

    //    //std::vector<EntityRaycastHit> hits = m_physicsManager.raycastAll(feetRay, filter);

    //    if (/*!hits.empty() || */physicsCharacter->groundState == PhysicsCharacter::GroundState::OnGround)
    //    {
    //        controller.jumping = false;
    //        controller.jumping_cooldown = 0.0f;
    //    } // If there are hits, we are in the ground
    //} // If the jumping cooldown is done, we need to check if we can jump again


    // We have done:
    // 1) Gotten all Components necessary
    // 2) Flattened camera forward
    // 3) Gotten direction input (based on camera forward)
    // 4) Projected the direction input based on the physicsCharacter's current ground normal
    // 
    // 
    // We have:
    // horizontalMoveDir: the direction (analog) where we want to go
    // moveAmount: the amount of move (of the total) we want to move
    // isMoving: tells us if we are moving (moveAmount > 0)
    // 
    // We need to 
    // 1) Update the Controller MoveState based on input
    //  1.5) update the ShapeHandle if we're crouching or walking
    // 2) Update the rest of the controller state
    //  2.1) Update the isGrounded based on the physics controller (or we could manually raycast, but the physics already does that and more)
    //  2.2) Check if player is aiming using input
    // 3) Get the current physics character velocity
    //  3.5) Get the current max speed
    // 4) Apply friction
    // 5) Apply acceleration (if we have input movement)
    // 6) Do extra
    //  6.1) Jump if able
    // 7) UPDATE ANIMATIONS

    // 1) Update Controller MoveState
    UpdateMoveState(controller, input, isMoving, *physicsCharacter);
    // 1.5) Update shape if needed (DO NOT DO YET)
    //ShapeHandle targetShape;
    //if (controller.state == C_PlayerController::Crouch || controller.state == C_PlayerController::ForcedSlide)
    //    targetShape = controller.crouchingShape;
    //else
    //    targetShape = controller.standingShape;
    //if(physicsCharacter->body && physicsCharacter->body->shapeHandle != targetShape)
    //    physics.setBodyShape(playerID, targetShape);
    
    // 2) Update the rest of the controller state
    //  2.1) Update the isGrounded based on the physics controller (or we could manually raycast, but the physics already does that and more)
    controller.isGrounded = physicsCharacter->groundState != PhysicsCharacter::InAir;
    if (physicsCharacter->groundState == PhysicsCharacter::OnGround)
        controller.jumping = false;
    //  2.2) Check if player is aiming using input
    bool isAiming = input.aim;

    // 3) Get the current physics character velocity
    controller.velocity = physicsCharacter->getRelativeVelocity();

    //  3.5) Get the current profile and adjust max speed
    auto& moveProfile = controller.GetProfile();
    float maxSpeed = moveProfile.maxSpeed;
    if (input.aim && controller.state == controller.Walk)
        maxSpeed *= 0.5f;

    
    // 4) Apply friction
    // Quick hack for decelerating based on friction when changing maxSpeeds
    // We first project the speed and check if we can keep accelerating
    float projectedSpeed = glm::dot(controller.velocity, desiredDir);
    // diff should be positive if we want there not to be forward friction
    float diff = maxSpeed - projectedSpeed;

    ApplyFriction(controller, moveProfile.friction, physicsCharacter->groundNormal, desiredDir, isMoving && (diff > 0.0f), dt);
    // 5) Apply acceleration (if we have input movement)
    if (isMoving)
        Accelerate(controller, desiredDir, maxSpeed, moveProfile.acceleration, dt);
    // 6) Do extra
    //  6.1) Jump if able
    if (controller.isGrounded)
    {
        if (input.jump && !controller.jumping)
        {
            physicsCharacter->jumping = true; // very important
            controller.jumping = true;

            controller.velocity += physics.getWorldUp() * controller.jumpForce;
        }

        // As an extra, if we are sliding we should be able to jump extra 
    }

    // IMPORTANT PART: UPDATE THE VELOCITY OF THE PHYSICS CHARACTER
    physicsCharacter->setVelocity(controller.velocity);

    // 7) UPDATE ANIMATIONS

    C_AnimatedMesh& animatedMesh = ecs.GetComponent<C_AnimatedMesh>(playerID);
    // Play animations
    if (ecs.Has<C_AnimatedMesh>(playerID))
    {
        animatedMesh.playbackSpeed = 1.0f;

        //jumping animations
        if (controller.jumping)
        {
            int jumpAnimId = GetAnimationIdFromName(animatedMesh, "pistoljump");

            // start jump once
            if (animatedMesh.lowerBodyLayer.currentAnimation != jumpAnimId)
            {
                PlayAnim(animatedMesh, "pistoljump", 0.1f);
                SetLooping(animatedMesh, animatedMesh.lowerBodyLayer, false);
            }

            animatedMesh.playbackSpeed = 0.6f;

            float dur = GetAnimationDuration(animatedMesh, jumpAnimId);
            if (animatedMesh.lowerBodyLayer.currentAnimationTime >= dur)
            {
                animatedMesh.lowerBodyLayer.currentAnimationTime = dur;
            }
        }
        else if (isMoving)
        {
            const char* animStateName = "pistolwalk";
            if (input.aim)
            {
                if (input.move_forward && !input.move_backward)
                {
                    animStateName = "pistolwalk";
                }
                else if (input.move_backward && !input.move_forward)
                {
                    animStateName = "pistolwalkbackwards";
                }
                else if (input.move_left && !input.move_right)
                {
                    animStateName = "pistolstrafeleft";
                }
                else if (input.move_right && !input.move_left)
                {
                    animStateName = "pistolstraferight";
                }
                else
                {
                    animStateName = "pistolwalk";
                }
            }
            else
            {
                if (controller.state == C_PlayerController::Sprint)
                    animStateName = "pistolrun";
                else
                    animStateName = "pistolwalk";
            }

            int moveAnimId =GetAnimationIdFromName(animatedMesh, animStateName);
            if (moveAnimId != -1 && animatedMesh.lowerBodyLayer.currentAnimation != moveAnimId)
            {
                PlayAnim(animatedMesh, animStateName, 0.15f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
        else //idle animations
        {
            int idleAnimId = GetAnimationIdFromName(animatedMesh, animatedMesh.idleAnimationName);
            if (animatedMesh.lowerBodyLayer.currentAnimation != idleAnimId)
            {
                PlayAnim(animatedMesh, animatedMesh.idleAnimationName, 0.25f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
    }

    // find equipped weapon and attach to hand
    ecs.GetView<C_Weapon, C_Transform>().ForEach([&](C_Weapon& weapon, C_Transform& weaponTransform)
        {
            if (!weapon.equipped)
                return;

            int handNodeIndex = -1;

            // find node for hand
            for (uint32_t i = 0; i < animatedMesh.asset->node_count; i++)
            {
                Node* node = &animatedMesh.asset->nodes[i];

                if (node->name && strcmp(node->name, "mixamorig:RightHand") == 0)
                {
                    handNodeIndex = (int)i;
                    break;
                }
            }

            glm::mat4 handMatrix = animatedMesh.joint_matrices[handNodeIndex];
            weaponTransform.matrix = transform.matrix * handMatrix;
        });
}

void PlayerMovementSystem::UpdateMoveState(C_PlayerController& controller, const C_PlayerInput& input, bool isPlayerMoving, const PhysicsCharacter& physicsCharacter)
{
    bool isRunning = input.run && isPlayerMoving && !input.aim;
    bool isCrouching = input.crouch;

    // force slide
    if (physicsCharacter.groundState == PhysicsCharacter::OnSteepGround)
    {
        controller.state = C_PlayerController::ForcedSlide;
        return;
    }

    // define priority here. crouching -> sprinting -> walking
    if (isCrouching)
        controller.state = C_PlayerController::Crouch;
    else if (isRunning)
        controller.state = C_PlayerController::Sprint;
    else
        controller.state = C_PlayerController::Walk;
        
}

// Accelerate function. SHOULD be called after getting the current velocity from the physics engine
void PlayerMovementSystem::Accelerate(C_PlayerController& controller, const glm::vec3& desiredDir, float maxSpeed, float acceleration, float dt)
{
    // We first project the speed and check if we can keep accelerating
    float projectedSpeed = glm::dot(controller.velocity, desiredDir);

    // AddSpeed will be the difference 
    float addSpeed = maxSpeed - projectedSpeed; // we will work with negative speed
    if (addSpeed <= 0.0f) return;

    // Interpolate so we never go further than the maxSpeed
    float accelerationAmount = addSpeed * (1.0f - std::exp(-acceleration * dt));
    controller.velocity += desiredDir * accelerationAmount;
}

// Game-side friction
// This should be called after the velocity is projected to the physics character's groundnormal, that get we get an appropiate friction
void PlayerMovementSystem::ApplyFriction(
    C_PlayerController& controller, 
    float friction, 
    const glm::vec3& groundNormal, 
    const glm::vec3& desiredDir, 
    bool hasInput, // has input is the same as isMoving, but more understandable
    float dt)
{
    float speedAlongNormal = glm::dot(controller.velocity, groundNormal);
    glm::vec3 tangentialVelocity = controller.velocity - groundNormal * speedAlongNormal;

    float speed = glm::length(tangentialVelocity);
    // if the speed is very little, slow down to 0 to avoid permanently drifting
    if (speed < 0.001f) { controller.velocity -= tangentialVelocity; return; }

    if (hasInput)
    {
        // If we have input, make the friction correct the direction (lateral movement), but not the forward movement, that way
        // we can properly define a maxSpeed with proper acceleration
        // (it was slowing me down...)
        float forwardSpeed = glm::dot(tangentialVelocity, desiredDir);
        glm::vec3 forwardVelocity = desiredDir * forwardSpeed;
        glm::vec3 lateralVelocity = tangentialVelocity - forwardVelocity;

        // Only add friction to the lateral component
        float lateralSpeed = glm::length(lateralVelocity);
        if (lateralSpeed > 0.001f)
        {
            float newLateralSpeed = glm::max(lateralSpeed - lateralSpeed * friction * dt, 0.0f);
            controller.velocity -= lateralVelocity * (1.0f - newLateralSpeed / lateralSpeed);
        }

    }
    else
    {
        float newSpeed = glm::max(speed - speed * friction * dt, 0.0f); // linearly doing friction (we could do quadratic depending on speed)
        float scale = newSpeed / speed;

        // Leave the normal speed be, friction only on the tangent (we could probably change the friction depending on the kinetic energy like in the physics engine but we will simply do depending on current speed)
        controller.velocity = groundNormal * speedAlongNormal + tangentialVelocity * scale;
    }
}
