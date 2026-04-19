#ifndef PHYSICS_SHAPES_CAPSULE_H
#define PHYSICS_SHAPES_CAPSULE_H


#include "shape.h"

#include "glm/glm.hpp"

class CapsuleShape : public IShape
{
public:
	float radius;
	float halfHeight; // distance from center to each sphere's origin along local Y axis

public:
	explicit CapsuleShape(float radius, float halfHeight)
		: IShape(ShapeType::Capsule), radius(radius), halfHeight(halfHeight)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		// same as sphere but we add or substract halfHeight depending if local direction is looking up or down;
		glm::vec3 result = glm::normalize(direction) * radius;
		result.y += (direction.y >= 0.0f) ? halfHeight : -halfHeight;

		return result;
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		glm::vec3 localUp = orientation * glm::vec3(0.0f, 1.0f, 0.0f);

		glm::vec3 base = position + localUp * -halfHeight;
		glm::vec3 tip = position + localUp * halfHeight;

		return AABB(
			glm::vec3{ glm::min(base, tip) - glm::vec3(radius) },
			glm::vec3{ glm::max(base, tip) + glm::vec3(radius) }
		);
	}

};

#endif // !PHYSICS_SHAPES_CAPSULE_H