#ifndef GAME_SYSTEMS_ENEMY_AI_SYSTEM_H
#define GAME_SYSTEMS_ENEMY_AI_SYSTEM_H

#include "System.h" // careful cause there is a <system.h>

// Important distinction: AISystems will just write onto other components, nothing else

class EnemyAISystem : public System
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
	void UpdateState			(EntityID enemyID, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const;
	bool ShouldChaseOrGetAlerted(EntityID enemyID, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const;
	bool ShouldAttack			(EntityID enemyID, C_EnemyAIInfo& info, const C_RigidBody& bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, float dt) const;

	bool IsEntityVisible(RigidBodyHandle enemyHandle, C_EnemyAIInfo& info, const glm::vec3& position, RigidBodyHandle targetHandle, const glm::vec3& targetPosition) const;
};

#endif //!GAME_SYSTEMS_ENEMY_AI_SYSTEM_H
