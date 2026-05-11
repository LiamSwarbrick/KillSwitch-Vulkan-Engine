#ifndef GAME_SYSTEMS_DESPAWN_SYSTEM_H
#define GAME_SYSTEMS_DESPAWN_SYSTEM_H

#include "System.h" // careful cause there is a <system.h>

// Important distinction: AISystems will just write onto other components, nothing else

class DespawnSystem : public System
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
		ecs->GetView<C_DespawnTimer>().ForEach([&](EntityID entity, C_DespawnTimer& despawnTimer)
			{
				despawnTimer.timer -= dt;

				if (despawnTimer.timer < 0.0f)
				{
					if (ecs->Has<C_RigidBody>(entity))
						physics->destroyBody(entity);

					ecs->DeleteEntity(entity);
				}
			});
	}
};

#endif //!GAME_SYSTEMS_DESPAWN_SYSTEM_H
