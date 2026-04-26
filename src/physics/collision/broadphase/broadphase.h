#ifndef PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H
#define PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H

#include "physics/core/types.h"

#include "grid.h"
//#include "quadtree.h"

#include <vector>

class BroadPhase
{
private:
	// To be taken away when we have an acceleration structure
	std::vector<RigidBody*> bodies;

public:
	explicit BroadPhase();
	//explicit BroadPhase(float cellSize, glm::vec3 gridWorldOrigin);
	~BroadPhase() = default;

	// Avoid copies
	BroadPhase(const BroadPhase&) = delete;
	BroadPhase& operator=(const BroadPhase&) = delete;


	// ----------------
	// BODY MANAGEMENT
	// ----------------
	void insert(RigidBody* body);
	void remove(RigidBody* body);

	// ----------------
	// QUERIES
	// ----------------
	void queryPairs(std::vector<BodyPair>& outPairs) const;

	void queryAABB(const AABB& aabb, const QueryFilter& filter, std::vector<RigidBody*> outBodies) const;
	void queryRay(const Ray& ray, const QueryFilter& filter, std::vector<RigidBody*> outBodies) const;
};

#endif // !PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H
