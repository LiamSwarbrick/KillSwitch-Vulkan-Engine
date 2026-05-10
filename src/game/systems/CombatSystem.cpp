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
            combatInfo.attackTimer -= dt;
            combatInfo.bufferTimer -= dt;
            combatInfo.windowTimer -= dt;

            bool shouldProcessMelee = false;
            bool shouldProcessRanged = false;

            // For firing we don't have buffered input, do NOT check
            // Check if we want ranged AND if we're able to shoot ranged (weapon socket)
            if (combatInfo.attackTimer < 0.0f && combatInput.wantsRanged && ecs->Has<C_WeaponSocket>(entity))
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
                // Check if we should process melee check the timers and buffers
                // If we are above the attackTimer
                if (combatInfo.attackTimer > 0.0f)
                {
                    // TODO: implement buffering damn
                    combatInfo.inputBuffered = true;
                }
                else
                {
                    shouldProcessMelee = true;
                }
            }

            if (shouldProcessMelee || shouldProcessRanged)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 position;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(transform.matrix, scale, rotation, position, skew, perspective);

                if (shouldProcessRanged)
                {
                    auto& socket = ecs->GetComponent<C_WeaponSocket>(entity);
                    C_WeaponRanged& weapon = ecs->GetComponent<C_WeaponRanged>(socket.weapon_entity);
                    ProcessRanged(bodyHandle.handle, position, combatInput.aimDir, combatInfo, weapon, C_Faction::FactionDamageMask(faction.type));
                }
                else if (shouldProcessMelee)
                {
                    glm::vec3 lookDir = Math::QuatToViewDir(rotation);
                    ProcessMelee(bodyHandle.handle, position, lookDir, combatInput, combatInfo, meleeStats, C_Faction::FactionDamageMask(faction.type));
                }
            }
            
        });

        
}

void CombatSystem::ProcessMelee(RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, const C_CombatInput& combatInput, C_CombatInfo& combatInfo, const C_CombatMeleeStats& meleeStats, FactionType damageMask) const
{
    if(combatInfo.activeCombo == -1)
        combatInfo.activeCombo = FindCombo(combatInput, meleeStats, combatInfo);

    if (combatInfo.activeCombo == -1)
        return;


    Shape* ourShape = physics->getShape(bodyHandle);
    CapsuleShape* ourCapsuleShape = static_cast<CapsuleShape*>(ourShape);



    // ShapeIntersects in front of us depending on meleeStats.range
    // We'll make a box as wide as our radius, half the height
    ShapeDesc shapeDesc = ShapeDesc::makeBox(glm::vec3(ourCapsuleShape->radius, ourCapsuleShape->getHeight() * 0.8f, meleeStats.range));
}

int CombatSystem::FindCombo(const C_CombatInput& combatInput, const C_CombatMeleeStats& meleeStats, const C_CombatInfo& combatInfo) const
{
    if (meleeStats.combos.empty())
    {
        SDL_assert(false && "We're trying to melee with an entity that has no attack combos (C_CombatMeleeStats::combos)");
        return -1;
    }

    if (meleeStats.combos.size() == 0) return 0;

    int bestMatch = -1;
    int bestScore = -1;
    
    // TODO: find the best match for the combo (its going to be 0 anyway for now that we only have 1 type of attack)

    return bestMatch;
}

void CombatSystem::ProcessRanged(RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& aimDir, C_CombatInfo& combatInfo, const C_WeaponRanged& weapon, FactionType damageMask) const
{
    // Write timers first of all because we might exit early
    combatInfo.isFiring = true;
    combatInfo.attackTimer = weapon.shootMaxCooldown;
    combatInfo.bufferTimer = 0.0f;
    combatInfo.windowTimer = 0.0f;

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
    if (cosAngle > 0.8f)
    {
        bodyRay.direction = glm::normalize(bodyToTarget);
        bodyRay.maxDistance = glm::length(bodyToTarget) + 0.5f; // camera is usually going to be behind the player so we might not need this
    }
    else
    {
        SDL_assert(false && "Checkpoint to see if it hits here, it should");
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
            }
        }
    }
    
}
