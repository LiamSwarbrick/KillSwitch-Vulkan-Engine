#ifndef PHYSICS_SHAPES_BOX_H
#define PHYSICS_SHAPES_BOX_H


#include "shape.h"

#include "glm/glm.hpp"

class BoxShape : public IShape
{
public:
	glm::vec3 halfWidths;

public:
	explicit BoxShape(glm::vec3 halfWidths)
		: IShape(ShapeType::Box), halfWidths(halfWidths)
	{
	}

	glm::vec3 support(glm::vec3 direction) const override
	{
		// WE ARE IN LOCAL-SPACE, GJK NEEDS TO TRANSFORM DIRECTION TO LOCAL-SPACE AND RESULT BACK TO WORLD-SPACE
		return glm::vec3{
			(direction.x >= 0 ? halfWidths.x : -halfWidths.x),
			(direction.y >= 0 ? halfWidths.y : -halfWidths.y),
			(direction.z >= 0 ? halfWidths.z : -halfWidths.z)
		};
	}

	AABB computeAABB(glm::vec3 position, glm::quat orientation) const override
	{
		// Project the box (OBB) axis into world axis
		// AABB' half-sizes on each world axis will be: ...
		// ... sum of projected OBB half-sizes
		glm::vec3 worldHalfSizes = orientation * halfWidths;

		return AABB(position - worldHalfSizes, position + worldHalfSizes);
	}

	RaycastHit intersectsRay(const Ray& ray, const glm::vec3& position, const glm::quat& orientation) const override
	{
        glm::mat3 rot = glm::mat3(orientation);
        glm::mat3 rotT = glm::transpose(rot);
        glm::vec3 localOrg = rotT * (ray.origin - position);
        glm::vec3 localDir = rotT * ray.direction;

        float tMin = 0.0f;
        float tMax = ray.maxDistance;
        int   normalAxis = -1;
        float normalSign = 1.0f;

        for (int i = 0; i < 3; i++)
        {

            // Edge case if direction is near 0.0f
            if (std::abs(localDir[i]) < F_EPSILON)
            {
                if (localOrg[i] < -halfWidths[i] ||
                    localOrg[i] >  halfWidths[i])
                    return RaycastHit::none();
            }
            else
            {
                float invD = 1.0f / localDir[i];
                float t0 = (-halfWidths[i] - localOrg[i]) * invD;
                float t1 = (halfWidths[i] - localOrg[i]) * invD;

                float sign = 1.0f;
                if (t0 > t1) 
                { 
                    std::swap(t0, t1); 
                    sign = -1.0f;
                }

                if (t0 > tMin)
                {
                    tMin = t0;
                    normalAxis = i;
                    normalSign = sign;
                }

                tMax = std::min(tMax, t1);

                if (tMax < tMin) return RaycastHit::none();
                if (tMax < 0.0f) return RaycastHit::none();
            }
        }

        if (normalAxis < 0) return RaycastHit::none();

        // Reconstruct world-space normal from hit axis
        glm::vec3 localNormal = glm::vec3(0.0f);
        localNormal[normalAxis] = normalSign;

        RaycastHit hit;
        hit.t = tMin;
        hit.point = ray.origin + ray.direction * tMin;
        hit.normal = rot * -localNormal;

        return hit;
	}

};

#endif // !PHYSICS_SHAPES_BOX_H