#ifndef PHYSICS_SHAPES_SPHERE_H
#define PHYSICS_SHAPES_SPHERE_H

#include "shape.h"

class SphereShape: public IShape
{
public:
	float radius;

public:
	explicit SphereShape(float radius)
		: IShape(ShapeType::Sphere), radius(radius)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		return glm::normalize(direction) * radius;
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		glm::vec3 sizes = glm::vec3(radius);

		return AABB(position - sizes, position + sizes);
	}

};

#endif // !PHYSICS_SHAPES_SPHERE_H