#ifndef PHYSICS_SHAPES_SHAPE_H
#define PHYSICS_SHAPES_SHAPE_H

#include "physics/core/types.h"
#include "physics/queries/raycast.h"

#include "glm/glm.hpp"

class IShape
{
public:

	virtual ~IShape() = default;

	ShapeType getType() const { return type; }

	// Function for GJK
	// Returns the furthest point given a direction vector from center of object 
	// IN LOCAL SPACE
	virtual glm::vec3 support(glm::vec3 direction) const = 0;

	// For later when broad-phase needs to be implemented

	// Used by broadphase to compute the AABB wrapping this shape
	// at a given world-space position and orientation
	virtual AABB computeAABB(glm::vec3 position, glm::quat orientation) const = 0;
	virtual RaycastHit intersectsRay(const Ray& ray, const glm::vec3& position, const glm::quat& orientation) const = 0;
	virtual float getHeight() const = 0;

public:
	glm::vec3 localOffset = glm::vec3(0.0f);
	glm::quat localOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	bool hasOffset() const
	{
		return localOffset != glm::vec3(0.0f)
			|| localOrientation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

protected:
	// explicit so we choose when to call it from child classes
	explicit IShape(ShapeType t) : type(t) {}

private:
	ShapeType type;
	// To have more data types on being inherited
};

#endif // !PHYSICS_SHAPES_SHAPE_H
