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

        float tMin = std::numeric_limits<float>::lowest();
        float tMax = std::numeric_limits<float>::max();

        int entryAxis = -1, exitAxis = -1;
        float entrySign = 1.0f, exitSign = 1;

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
                    entryAxis = i;
                    entrySign = sign;
                }
                if (t1 < tMax)
                {
                    tMax = t1;
                    exitAxis = i;
                    exitSign = sign;
                }

                if (tMax < tMin) return RaycastHit::none();
                if (tMax < 0.0f) return RaycastHit::none();
            }
        }

        bool inside = tMin < 0.0f;
        float t = inside ? tMax : tMin;
        if (t < F_EPSILON || t > ray.maxDistance) return RaycastHit::none();

        // Reconstruct world-space normal from hit axis
        glm::vec3 localNormal = glm::vec3(0.0f);
        if(inside)
            localNormal[exitAxis] = exitSign;
        else
            localNormal[entryAxis] = -entrySign;

        RaycastHit hit;
        hit.t = t;
        hit.point = ray.origin + ray.direction * t;
        hit.normal = rot * localNormal;

        return hit;
	}

};

#endif // !PHYSICS_SHAPES_BOX_H