#ifndef GAME_SYSTEMS_ZOMBIE_AI_SYSTEM_H
#define GAME_SYSTEMS_ZOMBIE_AI_SYSTEM_H

#include "System.h" // careful cause there is a <system.h>

// Important distinction: AISystems will just write onto other components, nothing else

class ZombieAISystem : public System
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
};

#endif //!GAME_SYSTEMS_ZOMBIE_AI_SYSTEM_H
