#ifndef GAME_SYSTEMS_PLAYER_INPUT_SYSTEM_H
#define GAME_SYSTEMS_PLAYER_INPUT_SYSTEM_H

#include "System.h"

class PlayerInputSystem : public System
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

#endif //!GAME_SYSTEMS_PLAYER_INPUT_SYSTEM_H
