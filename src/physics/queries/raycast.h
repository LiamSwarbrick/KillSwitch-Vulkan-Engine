#ifndef PHYSICS_QUERIES_RAYCAST_H
#define PHYSICS_QUERIES_RAYCAST_H

#include "glm/glm.hpp"

#include "physics/core/types.h"

#include <limits>

struct Ray
{
	glm::vec3 origin;
	glm::vec3 direction; // Direction vector in space, not orientation

	// Important addition to have a maxDistance attribute
	float maxDistance = std::numeric_limits<float>::max();
};

struct RigidBody; // Forward declare

struct RaycastHit
{
	glm::vec3 point; // o .  world-space
	glm::vec3 normal; // surface normal

	float t = -1.0f;
	RigidBody* body = nullptr;

	bool isValid() const { return t >= 0.0f; }

	static RaycastHit none() { return {}; }
};

#endif // !PHYSICS_QUERIES_RAYCAST_H