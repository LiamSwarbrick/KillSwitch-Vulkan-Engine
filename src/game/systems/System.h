#ifndef GAME_SYSTEMS_SYSTEM_H
#define GAME_SYSTEMS_SYSTEM_H

// Systems defined game-side because we need access to the physics and other components (for now physics?)
#include "core/ecs.h"
#include "physics/physics_manager.h"

// Also including the components so all systems don't need to include separately
#include "game/foundations/components.h"


// All the things a system might need.
// Every system will define them locally, see ZombieAISystem for an example
struct SystemContext
{
	ECS* ecs = nullptr;
	PhysicsManager* physicsManager = nullptr;
	// If we had other managers like audio, particle system, etc, anything we MIGHT need in the systems
	// ParticleManager* particleManager = nullptr;
	// AudioManager* audioManager = nullptr;
};

class System
{
public:
	virtual void Init(const SystemContext& ctx) = 0;
	virtual void Update(float dt) const = 0;
	virtual ~System() = default;
};


#endif // !GAME_SYSTEMS_SYSTEM_H

