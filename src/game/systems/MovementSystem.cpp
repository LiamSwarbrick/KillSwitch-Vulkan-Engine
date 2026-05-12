#include "MovementSystem.h"

#include "foundations/components.h"
#include "core/animation.h"


void MovementSystem::Update(float dt) const
{
    // BIG IMPORTANT NOTE(jaime):
    // WE NEED A fookin' class for this. if the physicsCharacter "crouches" we NEED to have a smaller capsule, 
    // and i would appreciate if we had all IShapes stored in advance instead of creating them on the fly
    // (PLEASE ALL CHARACTER SHAPES HAVE TO BE capsules please i beg you, otherwise physics die )
    ecs->GetView<C_MovementInput, C_MovementStats, C_MovementInfo>()
        .ForEach([&](EntityID characterID, C_MovementInput& moveInput, C_MovementStats& moveStats, C_MovementInfo& moveInfo)
    {
        // READ: Transform, MovementInput, MovementStats
        // WRITE: MovementInfo, PhysicsManager's character

        if (!ecs->Has(characterID)) return;

        PhysicsCharacter* physicsCharacter = physics->getCharacter(characterID);
        if (!physicsCharacter) return;

        glm::vec3 desiredDir = moveInput.desiredDir;
        float moveAmount = moveInput.moveAmount;
        bool isMoving = moveAmount > 0.0f;
        moveInfo.isMoving = isMoving;

        if (isMoving)
        {
            // Project horizontal direction to character's slope normal
            glm::vec3 slopeRight = glm::cross(desiredDir, physicsCharacter->groundNormal);
            glm::vec3 slopeForward = glm::cross(physicsCharacter->groundNormal, slopeRight);

            //if (slopeForward.y > 0.0f) slopeForward.y = 0.0f; // Do this to walk down surfaces correctly.
            //slopeForward.y = 0.0f;

            desiredDir = slopeForward;
        }

        // We have done:
        // 0) Projected the direction input based on the physicsCharacter's current ground normal
        // We have:
        // desiredDir: the direction in which we're moving
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
        UpdateMoveState(moveInfo, moveStats, moveInput, isMoving, *physicsCharacter);
        // 1.5) Update shape if needed (DO NOT DO YET)
        //ShapeHandle targetShape;
        //if (controller.state == C_PlayerController::Crouch || controller.state == C_PlayerController::ForcedSlide)
        //    targetShape = controller.crouchingShape;
        //else
        //    targetShape = controller.standingShape;
        //if(physicsCharacter->body && physicsCharacter->body->shapeHandle != targetShape)
        //    physics->setBodyShape(playerID, targetShape);

        // 2) Update the rest of the controller state
        //  2.1) Update the isGrounded based on the physics controller (or we could manually raycast, but the physics already does that and more)
        moveInfo.isGrounded = physicsCharacter->groundState != PhysicsCharacter::InAir;
        if (physicsCharacter->groundState == PhysicsCharacter::OnGround)
            moveInfo.isJumping = false;

        //  2.2) Check if player is aiming using input
        bool isAiming = moveInput.wantsAim;

        // 3) Get the current physics character velocity
        moveInfo.velocity = physicsCharacter->getRelativeVelocity();

        //  3.5) Get the current profile and adjust max speed
        const MovementProfile* moveProfile = nullptr;
        if (moveInfo.isGrounded)
            moveProfile = &moveStats.GetGroundProfile(moveInfo.state);
        else
            moveProfile = &moveStats.GetGroundProfile(moveInfo.state);


        float maxSpeed = moveProfile->maxSpeed * moveAmount;
        if (moveInput.wantsAim && moveInfo.state == MoveState::Walk)
            maxSpeed *= 0.5f;


        // 4) Apply friction
        // Quick hack for decelerating based on friction when changing maxSpeeds
        // We first project the speed and check if we can keep accelerating
        float projectedSpeed = glm::dot(moveInfo.velocity, desiredDir);
        // diff should be positive if we want there not to be forward friction
        float diff = maxSpeed - projectedSpeed;

        ApplyFriction(moveInfo, moveProfile->friction, physicsCharacter->groundNormal, desiredDir, isMoving && (diff > 0.0f), dt);
        // 5) Apply acceleration (if we have input movement)
        if (isMoving)
            Accelerate(moveInfo, desiredDir, maxSpeed, moveProfile->acceleration, dt);
        // 6) Do extra
        //  6.1) Jump if able
        if (moveInfo.isGrounded)
        {
            if (moveInput.wantsJump && !moveInfo.isJumping)
            {
                physicsCharacter->jumping = true; // very important
                moveInfo.isJumping = true;

                moveInfo.velocity += physics->getWorldUp() * moveStats.jumpForce;
            }

            // As an extra, if we are sliding we should be able to jump extra 
        }

        // IMPORTANT PART: UPDATE THE VELOCITY OF THE PHYSICS CHARACTER
        physicsCharacter->setVelocity(moveInfo.velocity);
    });

}

void MovementSystem::UpdateMoveState(C_MovementInfo& moveInfo, const C_MovementStats& moveStats, const C_MovementInput& moveInput, bool isPlayerMoving, const PhysicsCharacter& physicsCharacter)
{
    bool isRunning = moveInput.wantsRun && isPlayerMoving && !moveInput.wantsAim;
    bool isCrouching = moveInput.wantsCrouch;

    // force slide
    if (physicsCharacter.groundState == PhysicsCharacter::OnSteepGround)
    {
        moveInfo.state = MoveState::ForcedSlide;
        return;
    }

    // define priority here. crouching -> sprinting -> walking
    if (isCrouching)
        moveInfo.state = MoveState::Crouch;
    else if (isRunning)
        moveInfo.state = MoveState::Sprint;
    else
        moveInfo.state = MoveState::Walk;
        
}

// Accelerate function. SHOULD be called after getting the current velocity from the physics engine
void MovementSystem::Accelerate(C_MovementInfo& moveInfo, const glm::vec3& desiredDir, float maxSpeed, float acceleration, float dt)
{
    // We first project the speed and check if we can keep accelerating
    float projectedSpeed = glm::dot(moveInfo.velocity, desiredDir);

    // AddSpeed will be the difference 
    float addSpeed = maxSpeed - projectedSpeed; // we will work with negative speed
    if (addSpeed <= 0.0f) return;

    // Interpolate so we never go further than the maxSpeed
    float accelerationAmount = addSpeed * (1.0f - std::exp(-acceleration * dt));
    moveInfo.velocity += desiredDir * accelerationAmount;
}

// Game-side friction
// This should be called after the velocity is projected to the physics character's groundnormal, that get we get an appropiate friction
void MovementSystem::ApplyFriction(
    C_MovementInfo& moveInfo,
    float friction, 
    const glm::vec3& groundNormal, 
    const glm::vec3& desiredDir, 
    bool hasInput, // has input is the same as isMoving, but more understandable
    float dt)
{
    float speedAlongNormal = glm::dot(moveInfo.velocity, groundNormal);
    glm::vec3 tangentialVelocity = moveInfo.velocity - groundNormal * speedAlongNormal;

    float speed = glm::length(tangentialVelocity);
    // if the speed is very little, slow down to 0 to avoid permanently drifting
    if (speed < 0.001f) { moveInfo.velocity -= tangentialVelocity; return; }

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
            moveInfo.velocity -= lateralVelocity * (1.0f - newLateralSpeed / lateralSpeed);
        }

    }
    else
    {
        float newSpeed = glm::max(speed - speed * friction * dt, 0.0f); // linearly doing friction (we could do quadratic depending on speed)
        float scale = newSpeed / speed;

        // Leave the normal speed be, friction only on the tangent (we could probably change the friction depending on the kinetic energy like in the physics engine but we will simply do depending on current speed)
        moveInfo.velocity = groundNormal * speedAlongNormal + tangentialVelocity * scale;
    }
}

