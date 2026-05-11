#ifndef GAME_SYSTEMS_ANIMATION_SYSTEM_H
#define GAME_SYSTEMS_ANIMATION_SYSTEM_H

#include "System.h"

class AnimationSystem : public System
{
private:
	ECS* ecs = nullptr;
public:
	// Inherited via System
	void Init(const SystemContext& ctx) override
	{
		ecs = ctx.ecs;
	}

	void Update(float dt) const override;
private:
	void UpdatePlayer(float dt) const;
};


#endif //!GAME_SYSTEMS_ANIMATION_SYSTEM_H