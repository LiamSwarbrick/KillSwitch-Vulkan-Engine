#ifndef PHYSICS_SIMULATION_SOLVER_H
#define PHYSICS_SIMULATION_SOLVER_H

#include "physics/core/types.h"
#include "physics/collision/narrowphase/contact.h"

class Solver
{
public:
	void solve(const std::vector<Contact>& contacts, float dt);

private:
	inline void resolveInterpenetration(const Contact& contact, float dt);
	inline void resolveVelocities(const Contact& contact, float dt);
};

#endif // !PHYSICS_SIMULATION_SOLVER_H