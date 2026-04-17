#ifndef PHYSICS_COLLISION_NARROWPHASE_GJK_H
#define PHYSICS_COLLISION_NARROWPHASE_GJK_H

#include "physics/collision/shapes/shape.h"

#include "physics/core/types.h"

struct SimplexPoint
{
	glm::vec3 point; // Point of the Minkowski Difference
	glm::vec3 supportA; // Support point from shape A
	glm::vec3 supportB; // Support point from shape B

	SimplexPoint()
		: point(glm::vec3(0.0f)), supportA(glm::vec3(0.0f)), supportB(glm::vec3(0.0f)) {
	}

	SimplexPoint(glm::vec3 point, glm::vec3 supportA, glm::vec3 supportB)
		: point(point), supportA(supportA), supportB(supportB){
	}
};

struct Simplex
{
	SimplexPoint points[4];
	int size = 0;

	void clear()
	{
		size = 0;
	}

	// We will use add instead of push_front
	void add(const SimplexPoint& a)
	{
		SDL_assert(size < 4 && "Simplex can't have more than 4 points");
		points[size++] = a;
	}

	void push_front(const SimplexPoint& a)
	{
		SDL_assert(size < 4 && "Simplex can't have more than 4 points");
		points[3] = points[2];
		points[2] = points[1];
		points[1] = points[0];
		points[0] = a;
		size++;
	}

	// setters so you can create a simplex from nothing
	void set(const SimplexPoint& a)
	{
		points[0] = a;
		size = 1;
	}

	void set(const SimplexPoint& a, const SimplexPoint& b)
	{
		points[0] = a;
		points[1] = b;
		size = 2;
	}

	void set(const SimplexPoint& a, const SimplexPoint& b, const SimplexPoint& c)
	{
		points[0] = a;
		points[1] = b;
		points[2] = c;
		size = 3;
	}

	void set(const SimplexPoint& a, const SimplexPoint& b, const SimplexPoint& c, const SimplexPoint& d)
	{
		points[0] = a;
		points[1] = b;
		points[2] = c;
		points[3] = d;
		size = 4;
	}

	SimplexPoint& operator[](int i)
	{
		return points[i];
	}

	const SimplexPoint& operator[](int i) const
	{
		return points[i];
	}
};

struct GJKResult
{
	bool intersecting = false;

	Simplex simplex;

	// In case of not collision
	glm::vec3 closestPointOnA = glm::vec3(0.0f);
	glm::vec3 closestPointOnB = glm::vec3(0.0f);
	float distance = 0.0f;
};

// The positions and directions will be in world space, we need to:
// 1) Transform direction to each shape's local space, use shape.support(localDir)
// 2) Transform the result back to world space
SimplexPoint gjk_worldSupport(
	const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA,
	const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB,
	glm::vec3 worldDirection
);

GJKResult gjk_runGJK(
	const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA,
	const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB
);


#endif // !PHYSICS_COLLISION_NARROWPHASE_GJK_H
