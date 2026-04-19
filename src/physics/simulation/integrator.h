#ifndef PHYSICS_SIMULATION_INTEGRATOR_H
#define PHYSICS_SIMULATION_INTEGRATOR_H

#include "physics/core/types.h"

class Integrator
{
public:
	void integrate(RigidBody& body, float dt);

	float maxLinearVelocity = 100.0f;

private:
	inline void applyDamping(RigidBody& body, float dt);
	inline void integrateLinear(RigidBody& body, float dt);
	//inline void integrateAngular(RigidBody& body, float dt);
	inline void normalizeOrientation(RigidBody& body);
	inline void clampVelocities(RigidBody& body);

};

#endif // !PHYSICS_SIMULATION_INTEGRATOR_H