#include "integrator.h"

void Integrator::integrate(RigidBody& body, float dt)
{
	// Added sleep
	if (body.sleeping || body.isStatic || body.isTrigger || body.isKinematic) return;

	applyDamping(body, dt);
	integrateLinear(body, dt);
	normalizeOrientation(body);
}

inline void Integrator::applyDamping(RigidBody& body, float dt)
{
	body.velocity *= std::pow(body.damping, dt);
}

inline void Integrator::integrateLinear(RigidBody& body, float dt)
{
	glm::vec3 acceleration = body.forceAccumulator * body.invMass;

	// Semi-implicit euler
	// Add acceleration to the body's velocity
	body.velocity += acceleration * dt;
	clampVelocities(body);

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
