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
				bool isBeingDespawned = false;
				// If want a despawn timer, just add 
				//ecs->AddComponent<C_DespawnTimer>(entity, C_DespawnTimer{ .timer = 2.0f });

				if (health.currentHealth <= 0)
				{
					
					if (faction.type == FactionType::Player)
					{
						// Game over !!!!
						isBeingDespawned = true;
					}
					else if (faction.type == FactionType::Zombie)
					{
						isBeingDespawned = true;
					}
				}

				if (isBeingDespawned)
				{
					if (ecs->Has<C_RigidBody>(entity))
						physics->destroyBody(entity);

					ecs->DeleteEntity(entity);
				}

			});
	}
};

#endif //!GAME_SYSTEMS_HEALTH_SYSTEM_H

