#ifndef PHYSICS_SHAPES_CAPSULE_H
#define PHYSICS_SHAPES_CAPSULE_H


#include "shape.h"
#include "sphere.h"

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

	RaycastHit intersectsRay(const Ray& ray, const glm::vec3& position, const glm::quat& orientation) const override
	{
        glm::vec3 up = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 tip = position + up * halfHeight;
        glm::vec3 base = position + up * -halfHeight;
        glm::vec3 axis = tip - base;

        // Test infinite cylinder along the capsule axis
        glm::vec3  ao = ray.origin - base;
        glm::vec3  aod = cross(ao, axis);
        glm::vec3  d = cross(ray.direction, axis);

        float axisLenSq = glm::dot(axis, axis);
        float a = glm::dot(d, d);
        float b = 2.0f * glm::dot(d, aod);
        float c = glm::dot(aod, aod) - radius * radius * axisLenSq;
        float disc = b * b - 4.0f * a * c;

        if (disc < 0.0f) return RaycastHit::none();

        float t0 = (-b - std::sqrt(disc)) / (2.0f * a);
        float t1 = (-b + std::sqrt(disc)) / (2.0f * a);

        if (t0 > t1) std::swap(t0, t1);
        if (t0 < 0.0f) t0 = t1;

        float t = t0;

        if (t >= 0.0f && t <= ray.maxDistance)
        {
            glm::vec3  hitPoint = ray.origin + ray.direction * t;
            float proj = glm::dot(hitPoint - base, axis) / axisLenSq;

            if (proj >= 0.0f && proj <= 1.0f)
            {
                // Hit the cylindrical body
                glm::vec3 axisPoint = base + axis * proj;

                RaycastHit hit;
                hit.t = t;
                hit.point = hitPoint;
                hit.normal = glm::normalize(hitPoint - axisPoint);
                return hit;
            }
        }

        // Test hemisphere caps — each cap is a sphere at the endpoint
        SphereShape capSphere(radius);

        RaycastHit tipHit = capSphere.intersectsRay(ray, tip, glm::quat());
        RaycastHit baseHit = capSphere.intersectsRay(ray, base, glm::quat());

        if (tipHit.isValid() && baseHit.isValid())
            return tipHit.t < baseHit.t ? tipHit : baseHit;
        if (tipHit.isValid())  return tipHit;
        if (baseHit.isValid()) return baseHit;

        return RaycastHit::none();
	}

    float getHeight() const override
    {
        return radius * 2.0f + halfHeight * 2.0f;
    }

};

#endif // !PHYSICS_SHAPES_CAPSULE_H