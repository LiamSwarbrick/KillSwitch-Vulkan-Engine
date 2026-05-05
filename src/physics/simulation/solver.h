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
	friend struct ExtendedContact;

	void FillExtendedContact(const Contact& contact, ExtendedContact& outExtContact, float dt);

	void resolveInterpenetration(const ExtendedContact& contact, float dt);
	// Will be turned into full impulse solver for non-character bodies
	void resolveVelocities(const ExtendedContact& contact, float dt);

	void resolveCharacter(PhysicsCharacter& character, RigidBody& body, float dt);
	// Step Up will let Characters climb low obstacles if moving. Obstacle height defined in PhysicsCharacter::stepHeight
	// Good for climbing stairs if they are done using steps instead of a ramp
	bool tryStepUp(PhysicsCharacter& character, RigidBody& body, float dt);
	// Snap Down will make Characters snap down stairs or other obstacles
	bool trySnapDown(PhysicsCharacter& character, RigidBody& body, float dt);

private:
	PhysicsWorld& world;
	const float WALL_NORMAL_Y = 0.01f;
};

#endif // !PHYSICS_SIMULATION_SOLVER_H