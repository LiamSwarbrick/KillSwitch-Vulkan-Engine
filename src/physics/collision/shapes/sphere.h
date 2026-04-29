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

	RaycastHit intersectsRay(const Ray& ray, const glm::vec3& position, const glm::quat& orientation) const override
	{
		// Sphere is rotation-invariant — orientation unused
		// Resolve using quadratic formula
		glm::vec3  oc = ray.origin - position;
		float a = dot(ray.direction, ray.direction);
		float b = 2.0f * dot(oc, ray.direction);
		float c = dot(oc, oc) - radius * radius;
		float disc = b * b - 4.0f * a * c;

		if (disc < 0.0f) return RaycastHit::none();

		float t = (-b - std::sqrt(disc)) / (2.0f * a);

		if (t < 0.0f || t > ray.maxDistance)
			return RaycastHit::none();

		RaycastHit hit;
		hit.t = t;
		hit.point = ray.origin + ray.direction * t;
		hit.normal = glm::normalize(hit.point - position);
		return hit;
	}

};

#endif // !PHYSICS_SHAPES_SPHERE_H