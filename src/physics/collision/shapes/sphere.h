#ifndef PHYSICS_SHAPES_SPHERE_H
#define PHYSICS_SHAPES_SPHERE_H

#include "shape.h"

#include <cmath>

class SphereShape: public Shape
{
public:
	float radius;

public:
	explicit SphereShape(float radius)
		: Shape(ShapeType::Sphere), radius(radius)
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
		float oc2 = glm::dot(oc, oc);
		float r2 = radius * radius;
		float t, t0, t1;
#if 1
		// Geometrical solution
		float tc = glm::dot(oc, ray.direction);
		float d2 = oc2 - tc * tc;
		if (d2 > r2) return RaycastHit::none(); // Hit's outside the sphere

		float thc = sqrt(r2 - d2); // pythagoras

		t0 = tc - thc;
		t1 = tc + thc;
#else
		// Analytical solution

		// float a = dot(ray.direction, ray.direction); // assuming direction is normalized, a is 1
		float b = 2.0f * dot(oc, ray.direction);
		float c = oc2 - r2;
		float disc = b * b - 4.0f * c;

		if (disc < 0.0f) return RaycastHit::none();

		t0 = t1 = -b * 0.5f;

		float discSqrt = std::sqrt(disc);

		t0 = (-b - discSqrt) * 0.5f;
		t1 = (-b + discSqrt) * 0.5f;
#endif
		if (t0 > t1) std::swap(t0, t1);

		if (t0 < 0.0f) t0 = t1;

		t = t0;
		if (t < 0.0f || t > ray.maxDistance)
			return RaycastHit::none();

		RaycastHit hit;
		hit.t = t;
		hit.point = ray.origin + ray.direction * t;
		hit.normal = glm::normalize(hit.point - position);
		return hit;
	}

	float getHeight() const override
	{
		return radius * 2.0f;
	}

};

#endif // !PHYSICS_SHAPES_SPHERE_H