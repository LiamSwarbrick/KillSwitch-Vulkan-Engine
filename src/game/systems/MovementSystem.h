#ifndef GAME_SYSTEMS_MOVEMENT_SYSTEM_H
#define GAME_SYSTEMS_MOVEMENT_SYSTEM_H

#include "System.h"

#include "glm/glm.hpp"

class MovementSystem : public System
{
private:
    ECS* ecs = nullptr;
    PhysicsManager* physics = nullptr;

public:
    // Inherited via System
    void Init(const SystemContext& ctx) override
    {
        ecs = ctx.ecs;
        physics = ctx.physicsManager;
    }

    void Update(float dt) const override;

private:
	static void UpdateMoveState(C_MovementInfo& moveInfo, const C_MovementStats& stats, const C_MovementInput& moveInput, bool isPlayerMoving, const PhysicsCharacter& physicsCharacter);
	static void Accelerate(C_MovementInfo& moveInfo, const glm::vec3& desiredDir, float maxSpeed, float accel, float dt);
	static void ApplyFriction(
        C_MovementInfo& moveInfo,
        float friction,
        const glm::vec3& groundNormal,
        const glm::vec3& desiredDir,
        bool hasInput, 
        float dt);


};

#endif // !GAME_SYSTEMS_PLAYER_MOVEMENT_SYSTEM_H