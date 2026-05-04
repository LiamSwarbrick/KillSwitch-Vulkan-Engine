#include "gjk.h"

#include <limits>
#include <algorithm>


SimplexPoint gjk_worldSupport(
	const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA, 
	const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB, 
	glm::vec3 worldDirection)
{
	// Transform direction to local space (no scale for now)
	glm::vec3 localDirA = glm::inverse(oriA) * worldDirection;
	glm::vec3 localDirB = glm::inverse(oriB) * -worldDirection;

	// Get local support points
	glm::vec3 localSupportA = shapeA->support(localDirA);
	glm::vec3 localSupportB = shapeB->support(localDirB);

	// Transform supports from local to world space
	glm::vec3 worldSupportA = (oriA * localSupportA) + posA;
	glm::vec3 worldSupportB = (oriB * localSupportB) + posB;

	return SimplexPoint(
		worldSupportA - worldSupportB,
		worldSupportA,
		worldSupportB
	);
}

static inline bool gjk_sameDirection(const glm::vec3& a, const glm::vec3& b)
{
	return glm::dot(a, b) > 0.0f;
}

static void gjk_lineCase(Simplex& simplex, glm::vec3& direction)
{
	// a will be point 1, b will be point 0.
	SimplexPoint a = simplex[1];
	SimplexPoint b = simplex[0];

	glm::vec3 ab = b.point - a.point;
	glm::vec3 ao = -a.point;

	if (gjk_sameDirection(ab,ao))
	{
		//			ab x ao x ab
		direction = glm::cross(glm::cross(ab, ao), ab);
	}
	else
	{
		simplex.set(a);
		direction = ao;
	}

	return;
}

static void gjk_triangleCase(Simplex& simplex, glm::vec3& direction)
{
	/*
	* Clockwise (same as Casey's GJK video)
	*	A
	*	|\
	*	| \
	*	|  \
	*	|   \
	*	|    \
	*	C-----B
	*/
	SimplexPoint a = simplex[2];
	SimplexPoint b = simplex[1];
	SimplexPoint c = simplex[0];

	glm::vec3 ao = -a.point;
	glm::vec3 ab = b.point - a.point;
	glm::vec3 ac = c.point - a.point;
	glm::vec3 normal = glm::cross(ab, ac); // abc in casey's video

	if (gjk_sameDirection(glm::cross(normal, ac), ao))
	{
		// Towards ac normal

		if (gjk_sameDirection(ac, ao))
		{
			// Towards ac dir
			simplex.set(c, a);
			direction = glm::cross(glm::cross(ac, ao), ac);
		}
		else
		{
			simplex.set(b, a);
			gjk_lineCase(simplex, direction);
		}
	}
	else
	{
		// Away from ac normal
		if (gjk_sameDirection(glm::cross(ab, normal), ao))
		{
			simplex.set(b, a);
			gjk_lineCase(simplex, direction);
		}
		else
		{
			// Inside the triangle
			if (gjk_sameDirection(normal, ao))
			{
				// Towards triangle normal
				// simplex.set(c,b,a); // (current one)
				direction = normal;
			}
			else
			{
				// Away from triangle normal
				simplex.set(b, c, a);
				direction = -normal;
			}
		}
	}
}

static bool gjk_tetrahedronCase(Simplex& simplex, glm::vec3& direction)
{
	// 
	SimplexPoint a = simplex[3];
	SimplexPoint b = simplex[2];
	SimplexPoint c = simplex[1];
	SimplexPoint d = simplex[0];

	glm::vec3 ao = -a.point;
	glm::vec3 ab = b.point - a.point;
	glm::vec3 ac = c.point - a.point;
	glm::vec3 ad = d.point - a.point;

	glm::vec3 abc = glm::cross(ab, ac);
	glm::vec3 acd = glm::cross(ac, ad);
	glm::vec3 adb = glm::cross(ad, ab);

	// Only strict thing is to but the edge that A is not part of, to be C,B,A for the triangle (in clockwise order)
	// if ABC -> C B A
	if (gjk_sameDirection(abc, ao))
	{
		simplex.set(c, b, a);
		gjk_triangleCase(simplex, direction);
		return false;
	}

	if (gjk_sameDirection(acd, ao))
	{
		simplex.set(d, c, a);
		gjk_triangleCase(simplex, direction);
		return false;
	}

	if (gjk_sameDirection(adb, ao))
	{
		simplex.set(c, b, a);
		gjk_triangleCase(simplex, direction);
		return false;
	}

	return true;
}

bool gjk_doSimplex(Simplex& simplex, glm::vec3& direction)
{
	bool res = false;
	switch (simplex.size)
	{
	case 2: 
		gjk_lineCase(simplex, direction);
		break;
	case 3: 
		gjk_triangleCase(simplex, direction);
		break;
	case 4: 
		res = gjk_tetrahedronCase(simplex, direction);
		break;
	default:
		SDL_assert(false && "Invalid Simplex size");
	}

	return res;
}

// Ensure simplex has at least 2 points before calling this.
void gjk_findClosestPointIndexes(const Simplex& simplex, int& idxA, int& idxB)
{
	// Simple loop to find the 2 closest points of the simplex
	idxA = 0; idxB = -1;
	float closestA = glm::dot(simplex[0].point, simplex[0].point), closestB = std::numeric_limits<float>::max();
	float currentDistanceSquared;
	for (size_t i = 1; i < simplex.size; i++)
	{
		currentDistanceSquared = glm::dot(simplex[1].point, simplex[1].point);
		if (currentDistanceSquared < closestA)
		{
			idxB = idxA;
			closestB = closestA;
			idxA = i;
			closestA = currentDistanceSquared;
		}
		else if (currentDistanceSquared < closestB)
		{
			idxB = i;
			closestB = currentDistanceSquared;
		}
	}

	SDL_assert(idxB != -1 && "Illegal call on gjk find closest point");
}

void gjk_fillResultWithClosestPointAndDistance(GJKResult& gjk)
{
	int idxA = -1, idxB = -1;
	// Instead of this we could probably take the 2 latest points, but checking 4 points is no biggie
	gjk_findClosestPointIndexes(gjk.simplex, idxA, idxB);

	// Closest point on A and B are on simplex[idxA]
	gjk.closestPointOnA = gjk.simplex[idxA].supportA;
	gjk.closestPointOnB = gjk.simplex[idxA].supportB;

	// To get the distance, simply get the closest point of a segment to a point
	// Project the vector AO to AB and that's the distance
	glm::vec3 a = gjk.simplex[idxA].point;
	glm::vec3 b = gjk.simplex[idxB].point;

	glm::vec3 ab = b - a;
	glm::vec3 ao = /*(0,0,0)*/ -a;

	float abLengthSquared = glm::dot(ab, ab);
	float t = glm::dot(ao, ab) / abLengthSquared;
	
	t = std::clamp(t, 0.0f, 1.0f); // clamping t so it clamps at the segment ab

	glm::vec3 closestPoint = a + t * ab;

	// Distance is the length of the closestPoint to the origin
	gjk.distance = glm::length(closestPoint);
}

GJKResult gjk_runGJK(const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA, const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB)
{
	// GJK algorithm based on 
	GJKResult result;

	const int MAX_ITERATIONS = 64;

	// Initial search direction, posA - posB
	glm::vec3 direction = posA - posB;
	if (glm::length(direction) < F_EPSILON)
	{
		direction = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	
	SimplexPoint supportPoint = gjk_worldSupport(shapeA, posA, oriA, shapeB, posB, oriB, direction);
	result.simplex.add(supportPoint);

	direction = -supportPoint.point;

	for (int i = 0; i < MAX_ITERATIONS; i++)
	{
		supportPoint = gjk_worldSupport(shapeA, posA, oriA, shapeB, posB, oriB, direction);
		if (glm::dot(supportPoint.point, direction) < 0.0f)
		{
			result.intersecting = false;

			gjk_fillResultWithClosestPointAndDistance(result);
			return result;
		}

		result.simplex.add(supportPoint);

		if (gjk_doSimplex(result.simplex, direction))
		{
			result.intersecting = true;
			return result;
		}
	}

	
	result.intersecting = false;

	gjk_fillResultWithClosestPointAndDistance(result);
	return result;
}
