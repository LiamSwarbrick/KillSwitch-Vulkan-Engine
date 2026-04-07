#ifndef PHYSICS_SHAPES_BOX_H
#define PHYSICS_SHAPES_BOX_H


#include "shape.h"

#include "glm/glm.hpp"

class BoxShape : public IShape
{
public:
	glm::vec3 halfSizes;

public:
	explicit BoxShape(glm::vec3 halfSizes)
		: IShape(ShapeType::Box), halfSizes(halfSizes)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		// WE ARE IN LOCAL-SPACE, GJK NEEDS TO TRANSFORM DIRECTION TO LOCAL-SPACE AND RESULT BACK TO WORLD-SPACE
		return glm::vec3{
			(direction.x >= 0 ? halfSizes.x : -halfSizes.x),
			(direction.y >= 0 ? halfSizes.y : -halfSizes.y),
			(direction.z >= 0 ? halfSizes.z : -halfSizes.z)
		};
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		// Project the box (OBB) axis into world axis
		// AABB' half-sizes on each world axis will be: ...
		// ... sum of projected OBB half-sizes
		glm::vec3 worldHalfSizes = orientation * halfSizes;

		return AABB(position - worldHalfSizes, position + worldHalfSizes);
	}

};

#endif // !PHYSICS_SHAPES_BOX_H