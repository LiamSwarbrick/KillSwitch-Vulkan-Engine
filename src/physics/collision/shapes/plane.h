#ifndef PHYSICS_SHAPES_PLANE_H
#define PHYSICS_SHAPES_PLANE_H


#include "shape.h"

#include "glm/glm.hpp"
#include "SDL3/SDL.h"

class PlaneShape : public IShape
{
public:
	glm::vec3 normal;
	float distance; // distance from center to each sphere's origin along local Y axis

public:
	explicit PlaneShape(glm::vec3 normal, float distance)
		: IShape(ShapeType::Plane), normal(normal), distance(distance)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		// GJK should NOT be used for planes. we should use analytic tests.
		// only implemented so we complete the IShape interface
		return normal * distance + glm::normalize(direction) * std::numeric_limits<float>::max();
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		SDL_assert(false && "PlaneShape should NOT be in broadphase");
		return AABB{};
	}

};

#endif // !PHYSICS_SHAPES_PLANE_H