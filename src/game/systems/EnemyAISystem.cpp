#include "EnemyAISystem.h"

#include "core/utils/math_utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

void EnemyAISystem::Update(float dt) const
{
    auto view = ecs->GetView<C_Transform, C_EnemyAIStats, C_EnemyAIInfo, C_RigidBody, C_MovementInput, C_CombatInput, C_Faction>();
    view.ForEach([&](EntityID entity, C_Transform& transform, C_EnemyAIStats& stats, C_EnemyAIInfo& info, C_RigidBody& bodyHandle, C_MovementInput& moveInput, C_CombatInput& combatInput, C_Faction& faction)
        {
            // READ FROM: ZombieAIInfo and PhysicsManager
            // WRITE TO: ZombieAIInfo, MovementInput, CombatInput

            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 position;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transform.matrix, scale, rotation, position, skew, perspective);

            // Should be
            glm::vec3 lookDir = rotation * glm::vec3(0.0f, 0.0f, -1.0f);

            UpdateState(entity, stats, info, bodyHandle, position, lookDir, dt); // We might not need to pass entity (we have the position)

            // Before updating inputs, reset them
            moveInput.desiredDir = glm::vec3(0.0f);
            moveInput.moveAmount = 0.0f;
            moveInput.wantsRun = false;
            moveInput.wantsJump = false;
            moveInput.wantsCrouch = false;
            moveInput.wantsAim = false;
            moveInput.aimDir = glm::vec3(0.0f);
            moveInput.combatFactor = 0.0f;

            combatInput.aimDir = glm::vec3(0.0f);
            combatInput.wantsMelee = false;
            combatInput.wantsRanged = false;

            // After updating the state, lets now fill the inputs of the current state (they have to be updated from UpdateState)
            // Instead of a switch...
            if (info.currentState == info.Idle)
            {
                // Wait for idle till alerted or chase
                moveInput.moveAmount = 0.0f;

            }
            else if (info.currentState == info.Patrol)
            {
                // Patrol 
                // Changed if alerted or chase
                // TODO: move to patrolPoint, when patrolPoint reached, stay for a while, go to next patrolPoint

            }
            else if (info.currentState == info.Alerted)
            {
                // Look at the alert (should be written by another action bc it made an alert-able action (e.g. noise, walk, etc))
                moveInput.moveAmount = 0.0f;
                glm::vec3 targetFacingDir = glm::normalize(info.target - position);
                glm::quat targetRotation = Math::ViewDirToQuat(targetFacingDir);
                moveInput.aimDir = glm::normalize(info.target - position);

                // Quick fix until we get animations on the zombie
                //float yawDeg = glm::degrees(atan2f(targetFacingDir.x, targetFacingDir.z));
                //rotation = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));

                rotation = Math::RotateTowardTarget(rotation, targetRotation, stats.turnSpeed, dt, Math::Smoothstep);

                // We should probably have some turn speed

                transform.matrix = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

                // When alert cooldown reaches 0 go to Patrol or Idle
            }
            else if (info.currentState == info.Chase)
            {
                // Viciously chase the mf
                moveInput.moveAmount = 1.0f;
                glm::vec3 facingDir = glm::normalize(info.target - position);
                moveInput.desiredDir = facingDir;
                moveInput.wantsRun = true;
                // If vision with target lost, go to last seen target and remain alerted (or patrol between those 2 points or randomly)
            }
            else if (info.currentState == info.Attack)
            {
                // Attacking (written from C_Combat hopefully)
                // Wait for attack to finish, fall back to previous state (hopefully Chase)
            }
            else if (info.currentState == info.Staggered)
            {
                // Wait till staggered finish (Staggered written from C_Combat hopefully)
                // Wait for stagger to finish, fall back to previous state if Chase, otherwise Alerted towards the entity that hit it
                // That way if the zombie's hit on the back, once they stop being staggered they will go back to 

                // IMPORTANT: in case of hordes, i would recommend Staggered to apply a constant push (written to MovementInput), 
                // that way you might be able to displace zombies behind them (otherwise the zombie behind pushing forward will nullify the push backwards)
            }
            else if (info.currentState == info.Dead)
            {
                // Here in case the zombie should be a ragdoll (write null input at all), have a dead timer then despawn
                // Otherwise just despawn on health come to 0
            }

        });
}

void EnemyAISystem::UpdateState(EntityID enemyID, const C_EnemyAIStats& stats, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const
{
    info.stateTimer += dt;

    switch (info.currentState)
    {
    case info.Idle:
        // Wait for idle till alerted or chase
        if (ShouldChaseOrGetAlerted(stats, info, bodyHandle, position, lookDir, dt))
        {
            info.previousState = info.Idle;
            info.stateTimer = 0.0f;
        }

        break;
    case info.Patrol:
        // Patrol 
        // Changed if alerted or chase
        if (ShouldChaseOrGetAlerted(stats, info, bodyHandle, position, lookDir, dt))
        {
            info.previousState = info.Patrol;
        }

        break;
    case info.Alerted:
        // Look at the alert (should be written by another action bc it made an alert-able action (e.g. noise, walk, etc))
        if (ShouldChaseOrGetAlerted(stats, info, bodyHandle, position, lookDir, dt))
        {
            // ShouldChaseOrGetAlerted writes the current state
            if(info.currentState == info.Chase)
                info.previousState = info.Alerted;
        }

        // When alert cooldown reaches 0 go to Patrol or Idle

        break;
    case info.Chase:
        // Viciously chase the mf
        if (ShouldAttack(stats, info, bodyHandle, position, lookDir, dt))
        {
            info.previousState = info.Chase;
        }

        // If vision with target lost, go to last seen target and remain alerted (or patrol between those 2 points or randomly)
        break;
    case info.Attack:
        // Attacking (written from C_Combat hopefully)
        // Wait for attack to finish, fall back to previous state (hopefully Chase)
        break;
    case info.Staggered:
        // Wait till staggered finish (Staggered written from C_Combat hopefully)
        // Wait for stagger to finish, fall back to previous state if Chase, otherwise Alerted towards the entity that hit it
        // That way if the zombie's hit on the back, once they stop being staggered they will go back to 

        // IMPORTANT: in case of hordes, i would recommend Staggered to apply a constant push (written to MovementInput), 
        // that way you might be able to displace zombies behind them (otherwise the zombie behind pushing forward will nullify the push backwards)

        break;
    case info.Dead:
        // Here in case the zombie should be a ragdoll (write null input at all), have a dead timer then despawn
        // Otherwise just despawn on health come to 0

        break;
    default:
        break;
    }
}

// Returns true if should chase or get alerted. Writes the current 
bool EnemyAISystem::ShouldChaseOrGetAlerted(const C_EnemyAIStats& stats, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const
{
    glm::vec3 negatedPosition = -position;

    float closestTargetDistance = std::numeric_limits<float>::max();
    EntityID closestTargetID = NULL_ENTITY;
    glm::vec3 closestTargetPos = { 0,0,0 };

    bool shouldChase = false;
    bool shouldGetAlerted = false;

    // This is not optimized, we should probably order first by the least distance from enemyID to playerID, then work with that. but its ok.
    auto view = ecs->GetView<C_Transform, C_RigidBody, C_PlayerInput, C_MovementInfo>();
    view.ForEach([&](EntityID playerID, C_Transform& transform, C_RigidBody& playerHandle, C_PlayerInput& playerInput, C_MovementInfo& playerMoveInfo)
        {
            /*glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 playerPosition;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transform.matrix, scale, rotation, playerPosition, skew, perspective);*/

            glm::vec3 playerPosition = glm::vec3(transform.matrix[3]); // slight optimization bc we only want the player position

            glm::vec3 enemyToPlayer = playerPosition + negatedPosition;
            glm::vec3 enemyToPlayerDir = glm::normalize(enemyToPlayer);
            float distanceToPlayer = glm::length(playerPosition + negatedPosition);
        
            float enemyToPlayerAngle = glm::acos(glm::dot(enemyToPlayerDir, -lookDir)); // -lookDir cause its the other way around idk

            // Check if we have to chase is within the AI's vision
            // AND also if it is closer than the closestTargetDistance (early out)
            if (distanceToPlayer <= stats.visionDistance 
                && enemyToPlayerAngle <= stats.visionMaxAngle 
                && distanceToPlayer < closestTargetDistance)
            {
                // If it's within distance, raycast into player and check if we see him
                if (IsEntityVisible(bodyHandle.handle, info, position, playerHandle.handle, playerPosition))
                {
                    shouldChase = true;
                    // If the entity is visible, choose the closest player
                    closestTargetDistance = distanceToPlayer;
                    closestTargetID = playerID;
                    closestTargetPos = playerPosition;
                }
            }
            
            // If we are not chasing AND we're in range to be alerted 
            // AND the distance to the player is in alertDistance
            // (to be done) AND the player distance is in the player's move.alertDistance (this is to have sprint distance alert more than walking)
            // AND the player is moving
            // AND the player is not crouching
            if (!shouldChase
                && distanceToPlayer <= stats.alertDistance // to be changed to the player's sounds having alertDistance
                && distanceToPlayer < closestTargetDistance // (early out)
                && playerMoveInfo.isMoving
                && playerMoveInfo.state != MoveState::Crouch)
            {
                closestTargetDistance = distanceToPlayer;
                shouldGetAlerted = true;

                closestTargetPos = playerPosition;
            } 
        });

    if (shouldChase) 
    { 
        // Fill the target values
        info.target = closestTargetPos;
        info.hasTarget = true;
        info.activeTargetID = closestTargetID;

        info.currentState = info.Chase;
    }
    else if (shouldGetAlerted)
    {
        info.target = closestTargetPos;
        info.hasTarget = true;
        // No ID because we shouldn't know who it was

        info.currentState = info.Alerted;
    }
    else
    {
        info.hasTarget = true;
    }

    return (shouldChase || shouldGetAlerted);
}


bool EnemyAISystem::ShouldAttack(const C_EnemyAIStats& stats, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const
{
    // We should get the current target's position, and if it is in range we should attack
    // 
    // We could otherwise scan every player and choose the closest there is and check for an attack, but we will let the other functions change target
    if (info.activeTargetID == NULL_ENTITY)
    {
        SDL_assert(false && "We're checking if the AI should attack but it has no targetEntityID");
        return false;
    }

    // As we are using this after the physics engine, we know we can use C_Transform instead, but we might rather choose the RigidBody and check its position just in case
    C_Transform& targetTransform = ecs->GetComponent<C_Transform>(info.activeTargetID);
    Shape* targetShape = physics->getShape(info.activeTargetID);
    CapsuleShape* targetCapsule = static_cast<CapsuleShape*>(targetShape);
    float targetRadius = targetCapsule->radius;

    glm::vec3 targetPosition = glm::vec3(targetTransform.matrix[3]);

    glm::vec3 enemyToTarget = targetPosition - position;
    // We could also check the direction. If we had rotation speed we could get behind the zombie (at the time of writing this, enemy's rotation to target is instantaneous)
    float distanceToTarget = glm::length(enemyToTarget) - targetRadius;

    // If the target is within attack distance (WE SHOULD TAKE THE TARGET'S CAPSULE RADIUS INTO ACCOUNT)
    if (distanceToTarget <= stats.attackDistance)
    {
        info.currentState = info.Attack;
        return true;
    }

    return false;
}

bool EnemyAISystem::IsEntityVisible(RigidBodyHandle enemyHandle, C_EnemyAIInfo& info, const glm::vec3& position, RigidBodyHandle targetHandle, const glm::vec3& targetPosition) const
{
    // We get the shapes to calculate the heights and directions of the ray/s
    Shape* enemyShape = physics->getShape(enemyHandle);
    Shape* targetShape = physics->getShape(targetHandle);

    // Enemy head, position + halfHeight + shapeOffset - padding (0.1f)
    glm::vec3 enemyHead = position + glm::vec3(0.0f, enemyShape->getHeight() * 0.5f - 0.1f, 0.0f) + enemyShape->localOffset;

    // Target head, position + halfHeight + shapeOffset - padding (0.1f)
    //glm::vec3 targetHead = targetPosition + glm::vec3(0.0f, targetShape->getHeight() * 0.5f - 0.1f, 0.0f) + targetShape->localOffset;
    // Target head, position + shapeOffset
    glm::vec3 targetBody = targetPosition + targetShape->localOffset;
    // Target head, position - halfHeight + shapeOffset - padding (0.1f)
    //glm::vec3 targetFeet = targetPosition - glm::vec3(0.0f, targetShape->getHeight() * 0.5f + 0.1f, 0.0f) + targetShape->localOffset;

    // From the head of the enemy to the body of the target (we could do 3 different ones, head, body, toes)
    // Max distance will be the difference from enemy to target, so at least we should hit the target
    Ray headToBodyRay = {
        .origin = enemyHead,
        .direction = glm::normalize(targetBody - enemyHead),
        .maxDistance = glm::length(targetBody - enemyHead)
    };

    // Simple filter not to hit the enemy shooting the raycast
    QueryFilter filter = {
        .bodyToIgnore = enemyHandle
    };
    std::vector<EntityRaycastHit> hits = physics->raycastAll(headToBodyRay, filter);

    // We will filter the raycastHits (instead of calling entityRaycast)
    int closestIndex = -1;
    float closestT = std::numeric_limits<float>::max();
    int i = 0;
    for (const EntityRaycastHit& hit : hits)
    {
        // here you can manually skip any Entities by checking ECS

        // For example, we're going to exclude those entities that are zombies -> C_Faction.type == Zombie (because we still want to chase the player if we see it)
        if (ecs->Has<C_Faction>(hit.entity) && (ecs->GetComponent<C_Faction>(hit.entity).type == C_Faction::Zombie))
            continue;

        // Get the closest Hit that passes the filters
        if (hit.t < closestT)
        {
            closestT = hit.t;
            closestIndex = i;
        }

        i++;
    }
    // if (closestIndex == -1) return false; // should never be the case unless its empty, but should never be empty cause we should at least always hit the target
    EntityRaycastHit& closestHit = hits[closestIndex];
    
    // We return true if the closest filtered handle equals the target handle
    return closestHit.body->bodyID == targetHandle.index;
}
