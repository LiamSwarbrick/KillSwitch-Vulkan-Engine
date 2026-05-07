#ifndef PHYSICS_COLLISION_NARROWPHASE_EPA_H
#define PHYSICS_COLLISION_NARROWPHASE_EPA_H

#include "gjk.h"

struct EPAResult
{
	glm::vec3 pointA;
	glm::vec3 pointB;
	glm::vec3 point;
	glm::vec3 normal;
	float depth;

	bool converged; // if EPA converged in the 64 iterations
	// if not, it should still be treated as an approximation
};

EPAResult epa_runEPA(
	const Shape* shapeA, const glm::vec3& posA, const glm::quat& oriA,
	const Shape* shapeB, const glm::vec3& posB, const glm::quat& oriB,
	GJKResult& result
);

#endif // !PHYSICS_COLLISION_NARROWPHASE_EPA_H
