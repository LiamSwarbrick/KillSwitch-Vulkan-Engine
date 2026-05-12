#include "CombatSystem.h"

#include "core/utils/math_utils.h"

#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

// Have to include it here bc its not a class so we cannot add it to the System context
#include "game/ingame_cam.h"

void CombatSystem::Update(float dt) const
{
    auto view = ecs->GetView<C_Transform, C_RigidBody, C_CombatMeleeStats, C_CombatInput, C_CombatInfo, C_Faction>();
    view.ForEach([&](EntityID entity, C_Transform& transform, C_RigidBody bodyHandle, C_CombatMeleeStats& meleeStats, C_CombatInput& combatInput, C_CombatInfo& combatInfo, C_Faction& faction)
        {
            // READ FROM: CombatInput & CombatStats (for melee combos & damage)
            //              if ranged, get C_WeaponSocket, if not C_WeaponSocket fallback to melee
            // WRITE TO: C_CombatInfo !!!!!!

            // FIRST OF ALL, REDUCE THE TIMERS
            if (combatInfo.attackTimer >= 0.0f)
            {
                combatInfo.attackTimer -= dt;
            }
            // If we have an attack timer, we might have an attackDelayTimer
            if (combatInfo.attackDelayTimer >= 0.0f)
            {
                combatInfo.attackDelayTimer -= dt;
            }
            // Staggered should probably not be part of this
            if (combatInfo.staggeredTimer >= 0.0f)
            {
                combatInfo.staggeredTimer -= dt;
            }
            
            // Adding attackDelayTimer just in case an attack delay is bigger than animation time
            if (combatInfo.attackTimer < 0.0f && combatInfo.attackDelayTimer < 0.0f)
            {
                combatInfo.isFiring = false;
                combatInfo.isAttacking = false;
                combatInfo.hasAttacked = false;

                if (combatInfo.windowTimer >= 0.0f)
                {
                    combatInfo.windowTimer -= dt;
                }
            }
            if (combatInfo.staggeredTimer < 0.0f)
            {
                combatInfo.isStaggered = false;
            }


            // true when we get the first attack input. lower the delayTimer, once <0.0f then process the melee
            bool shouldPrepareMelee = false; 
            bool shouldProcessMelee = false;
            bool shouldProcessRanged = false;

            if (combatInfo.isAttacking && combatInfo.attackDelayTimer < 0.0f && !combatInfo.hasAttacked)
            {
                shouldProcessMelee = true; // processMelee will have max priority
            }

            // For firing we don't have buffered input, do NOT check
            // Check if we want ranged AND if we're able to shoot ranged (weapon socket)
            if (combatInfo.attackTimer < 0.0f)
            {
                if (combatInput.wantsRanged && ecs->Has<C_WeaponSocket>(entity))
                {
                    // Check if we have a weapon, then determine if we should process ranged attack
                    auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
                    // IF we have a weapon AND its equipped
                    shouldProcessRanged = (socket.weapon_entity != NULL_ENTITY && ecs->IsEntityValid(socket.weapon_entity))
                        && socket.equipped;

                    // If we should process ranged, we should check if we have bullets, and default to melee instead
                    if (shouldProcessRanged)
                    {
                        // Check the currently equipped weapon
                        auto& weapon = ecs->GetComponent<C_WeaponRanged>(socket.weapon_entity);
                        if (weapon.currentBullets <= 0)
                        {
                            // Process ranged to false so we process melee
                            shouldProcessRanged = false;
                        }
                    }
                }
                
                if (combatInput.wantsMelee && !shouldProcessRanged)
                {
                    shouldPrepareMelee = true;
                }
            }

            if (shouldProcessMelee)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 position;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(transform.matrix, scale, rotation, position, skew, perspective);

                glm::vec3 lookDir = Math::QuatToViewDir(rotation);

                ProcessMelee(entity, bodyHandle.handle, position, lookDir, combatInput, combatInfo, meleeStats, C_Faction::FactionDamageMask(faction.type));
            }
            else if (shouldProcessRanged)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 position;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(transform.matrix, scale, rotation, position, skew, perspective);


                auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
                C_WeaponRanged& weapon = ecs->GetComponent<C_WeaponRanged>(socket.weapon_entity);
                ProcessRanged(bodyHandle.handle, position, combatInput.aimDir, combatInfo, weapon, C_Faction::FactionDamageMask(faction.type));
            }
            else if (shouldPrepareMelee)
            {
                // Preparing the melee will set the timers to then process the melee attack
                PrepareMelee(combatInput, meleeStats, combatInfo);
            }

            
        });

        
}

void CombatSystem::PrepareMelee(const C_CombatInput& combatInput, const C_CombatMeleeStats& meleeStats, C_CombatInfo& combatInfo) const
{
    // If we do not have a current combo or the window timer has expired (for us to link the next attack)
    if (combatInfo.activeCombo < 0 || combatInfo.windowTimer < 0.0f)
    {
        combatInfo.activeCombo = FindCombo(combatInput, meleeStats, combatInfo);
        combatInfo.currentStep = 0;
    }
    else
    {
        // Increment the combo step if we have a combo and we have attacked in the window time
        combatInfo.currentStep++;
    }


    if (combatInfo.activeCombo == -1)
    {
        SDL_assert(false && "Could not find melee combo for the current entity");
        return;
    }

    // Prepare attack so it comes in handy 
    const C_CombatMeleeStats::Combo& currentCombo = meleeStats.combos[combatInfo.activeCombo];
    const C_CombatMeleeStats::Attack& currentAttack = currentCombo.attacks[combatInfo.currentStep];

    // SET THE ATTACK TIMERS 
    combatInfo.isAttacking = true;
    combatInfo.attackTimer = currentAttack.duration;
    combatInfo.attackDelayTimer = currentAttack.delay;
    combatInfo.windowTimer = currentAttack.comboWindow; // Set this now but do NOT decrement until we're done with the currentAttackTimer
}

void CombatSystem::ProcessMelee(EntityID ourID, RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, const C_CombatInput& combatInput, C_CombatInfo& combatInfo, const C_CombatMeleeStats& meleeStats, FactionType damageMask) const
{
    // Set hasAttacked so we don't attack again on the current attack.
    combatInfo.hasAttacked = true;

    // Prepare attack so it comes in handy 
    const C_CombatMeleeStats::Combo& currentCombo = meleeStats.combos[combatInfo.activeCombo];
    const C_CombatMeleeStats::Attack& currentAttack = currentCombo.attacks[combatInfo.currentStep];

    // Before doing anything else, reset the combo if we are on the last step (not the current one but the future one)
    if (combatInfo.currentStep == currentCombo.attacks.size() - 1)
    {
        combatInfo.activeCombo = -1;
    }

    // Get the shape of the current attacking entity to determine minimum attack range based on radius
    Shape* ourShape = physics->getShape(bodyHandle);
    CapsuleShape* ourCapsuleShape = static_cast<CapsuleShape*>(ourShape);
    float radiusOffset = ourCapsuleShape->radius;

    // ShapeIntersects in front of us depending on meleeStats.range
    // We'll make a box as wide as our radius, half the height
    //ShapeDesc attackShapeDesc = ShapeDesc::makeCapsule(meleeStats.range * 0.5f, ourCapsuleShape->getHeight());
    ShapeDesc attackShapeDesc = ShapeDesc::makeBox(glm::vec3(ourCapsuleShape->radius, ourCapsuleShape->getHeight() * 0.5f, meleeStats.range*0.5f));
    //ShapeDesc attackShapeDesc = ShapeDesc::makeSphere(meleeStats.range);
    ShapeHandle attackShapeHandle = physics->createShape(attackShapeDesc);

    // Create the filter (it will ignore us and target only dynamic entities)
    QueryFilter filter = {
        .bodyToIgnore = bodyHandle,
        .hasLayerOfQuery = true,
        .layerOfQuery = (uint8_t) BodyLayer::AFFECT_ONLY_CHARACTER
    };

    // Offset the target position by the radius in the look direction
    // And set the orientation to be the look direction we have at the moment 
    // TODO: to be able to aim upwards so we can hit a player that is up
    glm::quat targetOrientation = Math::ViewDirToQuat(lookDir);
    glm::vec3 targetPositionOffset = lookDir * (radiusOffset + meleeStats.range * 0.5f * currentAttack.rangeMultiplier);
    glm::vec3 targetPosition = position + targetPositionOffset;
    targetPosition.y += ourCapsuleShape->localOffset.y;

    std::vector<EntityShapeIntersectsHit> entitiesHit = physics->shapeIntersects(attackShapeHandle, targetPosition, targetOrientation, filter);


    float knockback = meleeStats.knockback * currentAttack.knockbackMultiplier;
    short damage = std::lroundf(((float)meleeStats.damage * currentAttack.damageMultiplier));
    float staggerTime = currentAttack.staggerTime;
    for (EntityShapeIntersectsHit hit : entitiesHit)
    {
        if (ecs->Has<C_Faction>(hit.entity))
        {
            //PlayGameplaySFXAt(&audio_system, gameplay_audio.zombie_attack, 0.72f, hit.point, 1.0f, 35.0f);
            auto& faction = ecs->GetComponent<C_Faction>(hit.entity);
            if (faction.type == (faction.type & damageMask))
            {
                C_Health& targetHealth = ecs->GetComponent<C_Health>(hit.entity);
                targetHealth.currentHealth -= damage;
                
                C_CombatInfo& targetCombatInfo = ecs->GetComponent<C_CombatInfo>(hit.entity);

                if (targetHealth.currentHealth <= 0)
                {
                    // Despawn the enemy? idk
                    // Add bullets back if we have a weapon
                    // Check if we have a weapon socket, if we do, check if we have a weapon, if we do, add bullets back
                    if (ecs->Has<C_WeaponSocket>(ourID))
                    {
                        auto& socket = ecs->GetComponent<C_WeaponSocket>(ourID);
                        // IF we have a weapon AND its equipped
                        if (socket.weapon_entity != NULL_ENTITY && ecs->IsEntityValid(socket.weapon_entity)) 
                        {
                            auto& weapon = ecs->GetComponent<C_WeaponRanged>(socket.weapon_entity);
                            weapon.reloadableBullets = std::min((short) (weapon.reloadableBullets + 1), weapon.maxBullets);
                        } // If our weapon socket has a valid entity
                    } // If we have a weapon socket


                    targetCombatInfo.isDead = true;
                } // If the target health is lower than 0 -> apply things
                else
                { // If the target is alive
                    // Only add stagger timer if the target is NOT staggered already
                    if(!targetCombatInfo.isStaggered)
                        targetCombatInfo.staggeredTimer = staggerTime;
                    targetCombatInfo.isStaggered = true;

                    physics->addVelocity(hit.entity, glm::vec3(lookDir.x + 0.0f, lookDir.y + 0.3f, lookDir.z + 0.0f) * knockback);
                }
            }
        }
    }
}

int CombatSystem::FindCombo(const C_CombatInput& combatInput, const C_CombatMeleeStats& meleeStats, const C_CombatInfo& combatInfo) const
{
    if (meleeStats.combos.empty())
    {
        SDL_assert(false && "We're trying to melee with an entity that has no attack combos (C_CombatMeleeStats::combos)");
        return -1;
    }

    if (meleeStats.combos.size() == 1) return 0;

    int bestMatch = -1;
    int bestScore = -1;
    
    // TODO: find the best match for the combo (its going to be 0 anyway for now that we only have 1 type of attack)

    return bestMatch;
}

void CombatSystem::ProcessRanged(RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& aimDir, C_CombatInfo& combatInfo, C_WeaponRanged& weapon, FactionType damageMask) const
{
    // Write timers first of all because we might exit early
    combatInfo.isFiring = true;
    combatInfo.attackTimer = weapon.shootMaxCooldown;
    combatInfo.bufferTimer = 0.0f;
    combatInfo.windowTimer = 0.0f;
    weapon.currentBullets -= weapon.shotsPerFire;

    const CameraInfo& cam = InGameCam_GetGameplayCamera();

    //PlayGameplaySFX(&audio_system, gameplay_audio.weapon_fire, 0.9f);
    // First raycast from to hit something, if we don't hit anything then assume we don't hit anything
    // Get the point we hit, and now cast a ray from the player's upperbody to the cameraRay's hit
    Ray cameraRay;
    cameraRay.origin = cam.position;
    cameraRay.direction = aimDir;
    cameraRay.maxDistance = 50.0f; // 50 as max distance, tweak

    QueryFilter filter = {};
    filter.bodyToIgnore = bodyHandle;
    filter.hasLayerOfQuery = false;

    EntityRaycastHit hit = physics->raycast(cameraRay, filter);
    if (!hit.isValid()) return; // Assume we hit nobody

    // IF we have hit something, shoot a ray from the position (upper body) in the direction of the hit
    Shape* shape = physics->getShape(bodyHandle);
    CapsuleShape* capsule = static_cast<CapsuleShape*>(shape);
    float height = capsule->getHeight();
    // Upperbody = bodyCenter (position.y + localoffset.y) + halfHeight (height * 0.5f) * valueThatYouThinkIsUpperBody
    float upperBodyHeight = position.y + shape->localOffset.y + (height * 0.5f) * 0.7f;


    glm::vec3 bodyRayOrigin = glm::vec3(position.x, upperBodyHeight, position.z);
    // we could get the hit.point, but i'd rather do it manually
    glm::vec3 cameraRayHitPostion = cameraRay.origin + cameraRay.direction * hit.t;
    SDL_Log("Camera raycast hit entity %s [%u] at point (%f, %f, %f)", ecs->GetEntityTag(hit.entity).c_str(), hit.entity, hit.point.x, hit.point.y, hit.point.z);
    glm::vec3 bodyToTarget = cameraRayHitPostion - bodyRayOrigin;

    // EDGE CASE: the camera hits an obstacle that's on the side of the player (because the camera is behind), choose camera dir instead
    float cosAngle = glm::dot(glm::normalize(bodyToTarget), aimDir);

    Ray bodyRay;
    bodyRay.origin = bodyRayOrigin;
    if (cosAngle > 0.6f)
    {
        bodyRay.direction = glm::normalize(bodyToTarget);
        bodyRay.maxDistance = glm::length(bodyToTarget) + 0.5f; // camera is usually going to be behind the player so we might not need this
    }
    else
    {
        // SDL_assert(false && "Checkpoint to see if it hits here, it should");
        bodyRay.direction = cameraRay.direction;
        bodyRay.maxDistance = cameraRay.maxDistance; // camera is usually going to be behind the player so we might not need this
    }


    hit = physics->raycast(bodyRay, filter);
    //bodyRay.direction
    if (!hit.isValid())
    {
        SDL_assert(false && "Camera got a raycast hit but not the body, weird, should have also hit");
        return;
    }
    {
        // TODO: Apply damage to hit entity, spawn hit effects, etc.

        if (ecs->Has<C_Faction>(hit.entity))
        {
            //PlayGameplaySFXAt(&audio_system, gameplay_audio.zombie_attack, 0.72f, hit.point, 1.0f, 35.0f);
            auto& faction = ecs->GetComponent<C_Faction>(hit.entity);
            if(faction.type == (faction.type & damageMask)) 
            {
                C_Health& targetHealth = ecs->GetComponent<C_Health>(hit.entity);
                targetHealth.currentHealth -= weapon.damage;

                if (targetHealth.currentHealth <= 0)
                {
                    // Upgrade could have extraBulletsOnKill or something like that
                    // or just reload the weapon.maxBullets
                    //weapon.reloadableBullets = weapon.maxBullets;
                    C_CombatInfo& targetCombatInfo = ecs->GetComponent<C_CombatInfo>(hit.entity);
                    targetCombatInfo.isDead = true;
                    weapon.reloadableBullets = std::min((short) (weapon.reloadableBullets + 1), weapon.maxBullets);
                }
            }
        }
    }
    
}
