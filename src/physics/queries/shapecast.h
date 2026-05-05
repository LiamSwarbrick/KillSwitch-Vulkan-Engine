#ifndef PHYSICS_QUERIES_SHAPECAST_H
#define PHYSICS_QUERIES_SHAPECAST_H

#include "glm/glm.hpp"

#include "physics/core/types.h"


struct ShapecastHit
{
	glm::vec3 point; // o .  world-space
	glm::vec3 pointA; // worldspace at t
	glm::vec3 pointB; // worldspace at t

	glm::vec3 normal; // surface normal (so B to A normal)

	float t = -1.0f;
	RigidBody* body = nullptr;

	bool isValid() const { return t >= 0.0f; }

	static ShapecastHit none() { return {}; }
};


#endif // !PHYSICS_QUERIES_SHAPECAST_H