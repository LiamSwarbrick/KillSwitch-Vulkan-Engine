#include "gjk.h"


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

GJKResult gjk_runGJK(const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA, const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB)
{
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
			result.distance = glm::length(result.simplex[result.simplex.size - 1].point);
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
	return result;
}
