#ifndef GAME_SYSTEMS_PLAYER_MOVEMENT_SYSTEM_H
#define GAME_SYSTEMS_PLAYER_MOVEMENT_SYSTEM_H

#include "core/ecs.h"
#include "physics/physics_manager.h"
#include "game/foundations/components.h"

#include "glm/glm.hpp"

class PlayerMovementSystem
{
public:
	static void Update(ECS& ecs, PhysicsManager& physics, EntityID playerID, const glm::vec3& forward, float dt);

private:
	static void UpdateMoveState(C_PlayerController& controller, const C_PlayerInput& input, bool isPlayerMoving, const PhysicsCharacter& physicsCharacter);
	static void Accelerate(C_PlayerController& controller, const glm::vec3& desiredDir, float maxSpeed, float accel, float dt);
	static void ApplyFriction(
        C_PlayerController& controller,
        float friction,
        const glm::vec3& groundNormal,
        const glm::vec3& desiredDir,
        bool hasInput, 
        float dt);
};

#endif // !GAME_SYSTEMS_PLAYER_MOVEMENT_SYSTEM_H