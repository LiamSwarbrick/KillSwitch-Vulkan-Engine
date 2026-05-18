#ifndef PHYSICS_SHAPES_PLANE_H
#define PHYSICS_SHAPES_PLANE_H


#include "shape.h"

#include "glm/glm.hpp"
#include "SDL3/SDL.h"

class PlaneShape : public Shape
{
public:
	glm::vec3 normal;
	float distance; // distance to origin along the normal

public:
	explicit PlaneShape()
		: Shape(ShapeType::Plane), normal(glm::vec3(0.0f)), distance(0.0f)
	{
	}

	explicit PlaneShape(glm::vec3 normal, float distance)
		: Shape(ShapeType::Plane), normal(normal), distance(distance)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		// GJK should NOT be used for planes. we should use analytic tests.
		// only implemented so we complete the Shape interface
		return normal * distance + glm::normalize(direction) * std::numeric_limits<float>::max();
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		SDL_assert(false && "PlaneShape should NOT be in broadphase");
		return AABB{};
	}

	RaycastHit intersectsRay(const Ray& ray, const glm::vec3& position, const glm::quat& orientation) const override
	{
		SDL_assert(false && "Not implemented, not using plane shapes");
		return RaycastHit::none();
	}

	float getHeight() const override
	{
		SDL_assert(false && "Not implemented, not using plane shapes");
		return 0.0f;
	}
};

#endif // !PHYSICS_SHAPES_PLANE_H