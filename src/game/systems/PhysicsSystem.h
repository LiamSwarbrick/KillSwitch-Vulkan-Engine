#ifndef GAME_SYSTEMS_PHYSICS_SYSTEM_H
#define GAME_SYSTEMS_PHYSICS_SYSTEM_H

#include "System.h"

class PhysicsSystem : public System
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

	void Update(float dt) const override
	{
		physics->update(*ecs, dt);
	}
};

#endif //!GAME_SYSTEMS_PLAYER_INPUT_SYSTEM_H
