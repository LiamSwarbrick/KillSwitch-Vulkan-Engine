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
private:

	inline void ChangeToReloading(C_PlayerInfo& playerInfo, const C_WeaponRanged& weapon) const
	{
		playerInfo.state = playerInfo.Reloading;
		playerInfo.reloadTimer = weapon.reloadTime / playerInfo.upgrades.extraReloadSpeed; // division :sob:
	}
};

#endif //!GAME_SYSTEMS_PLAYER_INPUT_SYSTEM_H
