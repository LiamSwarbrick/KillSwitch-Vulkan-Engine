#ifndef PHYSICS_SIMULATION_FORCE_GENERATOR_H
#define PHYSICS_SIMULATION_FORCE_GENERATOR_H

#include "physics/core/types.h"

// Force Generator Interface
class IForceGenerator
{
public:
	virtual ~IForceGenerator() = default;

	uint32_t targetLayers = (uint32_t)ForceLayer::Default;

	virtual void apply(RigidBody& body, float dt) = 0;
};

// Gravity generator
class GravityGenerator : public IForceGenerator
{
public: 
	glm::vec3 gravity = { 0, -9.81f, 0 };

	void apply(RigidBody& body, float dt) override
	{
		body.forceAccumulator += gravity * body.mass * body.gravityScale;
	}
};

// For winds, or other constant forces
class ConstantForceGenerator : public IForceGenerator
{
public:
	glm::vec3 force;

	void apply(RigidBody& body, float dt) override
	{
		body.forceAccumulator += force;
	}
};

// Might deprecate the following:
// For more complex (game-specific) on-the-fly uses
// Using lambda or other functions.
// But game-side SHOULD create their own generators inheriting from IForceGenerator
class CallbackForceGenerator : public IForceGenerator
{
public:
	std::function<void(RigidBody&, float dt)> callback;

	void apply(RigidBody& body, float dt) override
	{
		if (callback) callback(body, dt);
	}
};



#endif // !PHYSICS_SIMULATION_FORCE_GENERATOR_H
