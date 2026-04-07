#include "integrator.h"

void Integrator::integrate(RigidBody& body, float dt)
{
	if (body.isStatic || body.invMass <= 0.0f) return;
	if (body.isKinematic) return;

	// No sleep system for now, will check game engine's chapter when i have time

	applyDamping(body, dt);
	integrateLinear(body, dt);
	normalizeOrientation(body);
	clampVelocities(body);
}

inline void Integrator::applyDamping(RigidBody& body, float dt)
{
	// we could go for exponential decay, but we'll do linear for now
	body.velocity *= std::pow(body.damping, dt);
}

inline void Integrator::integrateLinear(RigidBody& body, float dt)
{
	glm::vec3 acceleration = body.forceAccumulator * body.invMass;

	// Semi-implicit euler
	body.velocity += acceleration * dt;
	body.position += body.velocity * dt;

	body.forceAccumulator = glm::vec3(0.0f);
}

inline void Integrator::normalizeOrientation(RigidBody& body)
{
	body.orientation = glm::normalize(body.orientation);
}

inline void Integrator::clampVelocities(RigidBody& body)
{
	if (body.velocity.length() > maxLinearVelocity)
	{
		body.velocity = glm::normalize(body.velocity) * maxLinearVelocity;
	}

	// To add angular when needed
}
