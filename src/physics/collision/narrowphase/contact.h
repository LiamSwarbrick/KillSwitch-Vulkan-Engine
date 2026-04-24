#ifndef PHYSICS_COLLISION_NARROWPHASE_CONTACT_H
#define PHYSICS_COLLISION_NARROWPHASE_CONTACT_H

#include "glm/glm.hpp"

#include "physics/core/types.h"

struct Contact
{
	glm::vec3 point; // in world-space
	glm::vec3 normal; // from B to A

	// Extras just to debug
	glm::vec3 pointA;
	glm::vec3 pointB; 

	float depth = -1.0f; // interpenetration depth (negative means invalid / no interpenetration)

	RigidBody* bodyA = nullptr;
	RigidBody* bodyB = nullptr;

	bool isValid() const { return depth >= 0.0f; }

	static Contact none() { return {}; }
};

#endif // !PHYSICS_COLLISION_NARROWPHASE_CONTACT_H