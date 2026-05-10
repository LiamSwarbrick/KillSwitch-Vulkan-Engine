#ifndef GAME_SYSTEMS_COMBAT_SYSTEM_H
#define GAME_SYSTEMS_COMBAT_SYSTEM_H

#include "System.h" // careful cause there is a <system.h>

// Important distinction: AISystems will just write onto other components, nothing else

class CombatSystem : public System
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
	void ProcessMelee(RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& lookDir, const C_CombatInput& combatInput, C_CombatInfo& combatInfo, const C_CombatMeleeStats& meleeStats, FactionType damageMask) const;
	void ProcessRanged(RigidBodyHandle bodyHandle, const glm::vec3& position, const glm::vec3& aimDir, C_CombatInfo& combatInfo, const C_WeaponRanged& weapon, FactionType damageMask) const;

	// We could add movement input to the combat & combos
	int FindCombo(const C_CombatInput& combatInput, const C_CombatMeleeStats& meleeStats, const C_CombatInfo& combatInfo) const;
};

#endif //!GAME_SYSTEMS_COMBAT_SYSTEM_H
