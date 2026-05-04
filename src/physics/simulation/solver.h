#ifndef PHYSICS_SIMULATION_SOLVER_H
#define PHYSICS_SIMULATION_SOLVER_H

#include "physics/core/types.h"
#include "physics/collision/narrowphase/contact.h"

// Forward declare
class PhysicsWorld;

class Solver
{
public:
	explicit Solver(PhysicsWorld& world)
		: world(world)
	{
	}

	void solve(const std::vector<Contact>& contacts, float dt);

private:
	// Definition in solver.cpp
	struct ExtendedContact;

	bool changeContactDependingOnCharacterWalkability(ExtendedContact& contact, float dt);

	void resolveInterpenetration(const ExtendedContact& contact, float dt);
	// Will be turned into full impulse solver for non-character bodies
	void resolveVelocities(const ExtendedContact& contact, float dt);

	void resolveCharacter(const ExtendedContact& contact, float dt);
	bool tryStepUp();
	bool tryStepDown();

private:
	PhysicsWorld& world;
};

#endif // !PHYSICS_SIMULATION_SOLVER_H