#include "gjk.h"

#include <limits>
#include <algorithm>


SimplexPoint gjk_worldSupport(
	const Shape* shapeA, const glm::vec3& posA, const glm::quat& oriA, 
	const Shape* shapeB, const glm::vec3& posB, const glm::quat& oriB, 
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

// returns true if the dot product between a,b is positive
static inline bool gjk_sameDirection(const glm::vec3& a, const glm::vec3& b)
{
	return glm::dot(a, b) > 0.0f;
}

static void gjk_lineCase(Simplex& simplex, glm::vec3& direction)
{	
	// We come from B, so we can only be at the segment AB, or after the segment, beyond A (no contribution of B)

	//        |     O      |  
	//  no O  B <--------- A   O
	//        |      O     |

	// a will be point 1, b will be point 0.
	SimplexPoint a = simplex[1];
	SimplexPoint b = simplex[0];


	glm::vec3 ab = b.point - a.point;
	glm::vec3 ao = -a.point;

	if (gjk_sameDirection(ab,ao))
	{
		// We are at the segment AB (meaning the projection (dot(ab,ao)) of the point ao on the segment is between (0,1))
		// We know it is clamped at 1 because we come from B, so B cannot be the only contributor to the closest distance.

		// We choose a direction perpendicular to the segment, in the direction of the origin
		direction = glm::cross(glm::cross(ab, ao), ab);
	}
	else
	{
		// Reset simplex, closest point is now simplex[0]
		simplex.set(a);
		// Direction is A -> origin
		direction = ao;
	}

	return;
}

static void gjk_triangleCase(Simplex& simplex, glm::vec3& direction)
{
	// We just added A, and the point IS inside the segment CB (so it cannot be in the opposite direction of A projected to CB)
	/*
	* Clockwise (same as Casey's GJK video)
	* 
	*       | \  5  / |
	*       |  \   /  |        
	*	    |   \A/   |
	*	    | 1 / \ 4 |
	*	    |  /   \  |
	* no O  | / 2   \ |  no O
	*	    |/    3  \|
	*	    C---------B  
	*		   no O
	* 
	* The figure is in 2D but it's 3D
	*/
	SimplexPoint a = simplex[2];
	SimplexPoint b = simplex[1];
	SimplexPoint c = simplex[0];

	glm::vec3 ao = -a.point;
	glm::vec3 ab = b.point - a.point;
	glm::vec3 ac = c.point - a.point;
	glm::vec3 normal = glm::cross(ab, ac); // triangle normal: abc in casey's video

	if (gjk_sameDirection(glm::cross(normal, ac), ao))
	{
		// We are towards AC's outward normal, so we can either be at 1, 5, OR 4 (4 depending on the angles)

		if (gjk_sameDirection(ac, ao))
		{
			// We are inside the AC segment, towards AC's outward normal, meaning we are at 1
			// Same as a segment case, we build the simplex out of C A
			simplex.set(c, a);
			// And set the direction from AC to the origin
			direction = glm::cross(glm::cross(ac, ao), ac);
		}
		else
		{
			// We can either be at 5, or 4 which ends up being the same as a line / segment (2 points) case
			// We build the simplex with A B (because we know we cannot be behind B looking from A)
			simplex.set(b, a);
			// And solve for the line case
			gjk_lineCase(simplex, direction);
		}
	}
	else
	{
		// We are AWAY FROM AC's outward normal, meaning we can either be inside the triangle (2,3), 4 or 5

		if (gjk_sameDirection(glm::cross(ab, normal), ao))
		{
			// We are towards AB's outward normal, meaning we can only be at 5, or 4
			// We build the simplex with A B (because we know we cannot be behind B looking from A)
			simplex.set(b, a);
			// And solve for the line case
			gjk_lineCase(simplex, direction);
		}
		else
		{
			// We are inside the triangle (2 or 3), we now have to check if we are towards or away from the triangle's normal
			if (gjk_sameDirection(normal, ao))
			{
				// We are towards the triangle normal
				// We build the simplex as C B A, and set the direction to be the normal
				// simplex.set(c,b,a); // (current one)
				direction = normal;
			}
			else
			{
				// We are away from the triangle normal
				// We build the simplex as B C A, and set the direction to be -normal
				simplex.set(b, c, a);
				direction = -normal;
			}
		}
	}
}

static bool gjk_tetrahedronCase(Simplex& simplex, glm::vec3& direction)
{
	// We just added A, and we cannot be behind BCD  triangle's normal (BC x BD)
	// This is NOT optimized, for it to be optimized we should not fall back into a triangle cases and work out all possible angles

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
		simplex.set(d, b, a); // holy bug how was it even converging??? (before edit it was c,b,a !?"=?!�=!?!?==!=! what was i on
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

void gjk_fillResultWithClosestPointAndDistance(GJKResult& gjk)
{
	// When exiting from here, we need to assume the last element added is the closest to the origin (or tied in distance with others) 
	if (gjk.simplex.size == 1)
	{
		gjk.closestPointOnA = gjk.simplex[0].supportA;
		gjk.closestPointOnB = gjk.simplex[0].supportB;
		gjk.distance = glm::length(gjk.simplex[0].point);
		gjk.distanceDirection = glm::normalize(gjk.simplex[0].point);

		return;
	}
	else if (gjk.simplex.size == 2)
	{
		// Last point added is A, if we exit on a segment, it means the origin is inside the segment,
		// and the search direction was on the direction normal to the segment
		glm::vec3 a = gjk.simplex[1].point;
		glm::vec3 b = gjk.simplex[0].point;

		glm::vec3 ab = b - a;
		glm::vec3 ao = /*(0,0,0)*/ -a;

		float abLengthSquared = glm::dot(ab, ab);
		float t = glm::dot(ao, ab) / abLengthSquared;

		SDL_assert((t <= 1.0f && t >= 0.0f) && "t falls off range from (0,1), meaning i was wrong tf");

		//t = std::clamp(t, 0.0f, 1.0f); // clamping t so it clamps at the segment ab

		glm::vec3 closestPoint = a + t * ab; // = (1-t) * a + t * b

		// Distance is the length of the closestPoint to the origin
		gjk.distance = glm::length(closestPoint);

		// lets try something extra and find the direction of the 2 closest points
		glm::vec3 closestA = (1 - t) * gjk.simplex[1].supportA + t * gjk.simplex[0].supportA;
		glm::vec3 closestB = (1 - t) * gjk.simplex[1].supportB + t * gjk.simplex[0].supportB;

		gjk.closestPointOnA = closestA;
		gjk.closestPointOnB = closestB;

		// We could use this or choose the normal of the segment (ab x ao x ab)
		gjk.distanceDirection = glm::normalize(closestB - closestA);
	}
	else if (gjk.simplex.size == 3)
	{
		// So if we exit here, we had to be searching in the direction of NORMAL of current ABC, A being idx 2, B being idx 1, C being idx 0
		// Meaning we need to do barycentric on ABC
		// ABC is clockwise
		// For Barycentric: Cramer's rule for solving a linear system 
		glm::vec3 a = gjk.simplex[2].point;
		glm::vec3 b = gjk.simplex[1].point;
		glm::vec3 c = gjk.simplex[0].point;


		glm::vec3 ab = b - a, ac = c - a, ao = /* 0,0,0 */ -a;
		float d00 = glm::dot(ab, ab);
		float d01 = glm::dot(ab, ac);
		float d11 = glm::dot(ac, ac);
		float d20 = glm::dot(ao, ab);
		float d21 = glm::dot(ao, ac);
		float invDenom = 1.0f / (d00 * d11 - d01 * d01);
		
		float v = (d11 * d20 - d01 * d21) * invDenom;
		float w = (d00 * d21 - d01 * d20) * invDenom;
		float u = 1.0f - v - w;

		glm::vec3 closestPoint = u * a + v * b + w * c;

		gjk.distance = glm::length(closestPoint);

		glm::vec3 closestA = u * gjk.simplex[2].supportA + v * gjk.simplex[1].supportA + w * gjk.simplex[0].supportA;
		glm::vec3 closestB = u * gjk.simplex[2].supportB + v * gjk.simplex[1].supportB + w * gjk.simplex[0].supportB;

		gjk.closestPointOnA = closestA;
		gjk.closestPointOnB = closestB;

		// We could do this OR choose the normal of the triangle (should be correct)

		gjk.distanceDirection = glm::normalize(closestB - closestA);
	}
	else
	{
		SDL_assert(false && "ok it should not be possible (with the current implementation) to return false and have a tetrahedron");
	}
	
}

GJKResult gjk_runGJK(const Shape* shapeA, const glm::vec3& posA, const glm::quat& oriA, const Shape* shapeB, const glm::vec3& posB, const glm::quat& oriB)
{
	// GJK algorithm based on Casey Muratori's video (non optimized at the tetrahedron)
	// Trying to optimize the calculation of the resulting distance based on each state
	GJKResult result;

	const int MAX_ITERATIONS = 64;

	// Initial search direction, posA - posB
	glm::vec3 direction = posA - posB;
	if (glm::length(direction) < F_EPSILON)
	{
		direction = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	
	// We get a support point based on the direction posB -> posA
	SimplexPoint supportPoint = gjk_worldSupport(shapeA, posA, oriA, shapeB, posB, oriB, direction);
	result.simplex.add(supportPoint);

	// Set the direction to the opposite direction of the resulting point
	direction = -supportPoint.point;

	for (int i = 0; i < MAX_ITERATIONS; i++)
	{
		supportPoint = gjk_worldSupport(shapeA, posA, oriA, shapeB, posB, oriB, direction);
		if (glm::dot(supportPoint.point, direction) < 0.0f)
		{
			// If there is no point in the direction we asked for
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
