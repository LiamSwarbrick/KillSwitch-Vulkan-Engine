#ifndef GAME_SYSTEMS_HEALTH_SYSTEM_H
#define GAME_SYSTEMS_HEALTH_SYSTEM_H

#include "System.h" // careful cause there is a <system.h>

// Important distinction: AISystems will just write onto other components, nothing else

class HealthSystem : public System
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
		ecs->GetView<C_Health, C_Faction>().ForEach([&](EntityID entity, C_Health& health, C_Faction& faction)
			{
				ecs->GetView<C_Health, C_Faction>().ForEach([&](EntityID entity, C_Health& health, C_Faction& faction)
					{
						if (health.currentHealth <= 0)
						{
							// make sure we only trigger once by checking if we already have the timer
							if (!ecs->Has<C_DespawnTimer>(entity))
							{
								if (faction.type == FactionType::Player)
								{
									ecs->AddComponent<C_DespawnTimer>(entity, C_DespawnTimer{ 3.0f });
								}
								else if (faction.type == FactionType::Zombie)
								{
									ecs->AddComponent<C_DespawnTimer>(entity, C_DespawnTimer{ 15.5f });
								}
							}
						}

					});
			});
	}
};
#endif //!GAME_SYSTEMS_HEALTH_SYSTEM_H

