#ifndef PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H
#define PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H

#include "physics/core/types.h"
#include "physics/queries/raycast.h"
#include "physics/queries/query_filter.h"
#include "physics/collision/filters/body_layer_filter.h"



#include "grid.h"
//#include "quadtree.h"

#include <vector>

class BroadPhase
{
private:
	// To be taken away when we have an acceleration structure
	// Accelleration structure should have 2 different ones, one for dynamic and other for static objects
	// Insertion and deletion (and lookup) would be much faster
	std::vector<RigidBody*> bodies;
	BodyLayerFilter* bodyLayerFilter;

public:
	explicit BroadPhase() {};
	explicit BroadPhase(BodyLayerFilter* bodyLayerFilter);
	//explicit BroadPhase(float cellSize, glm::vec3 gridWorldOrigin);
	~BroadPhase() = default;

	BroadPhase(const BroadPhase&) = delete;
	BroadPhase& operator=(const BroadPhase&) = delete;

	BroadPhase& operator=(BroadPhase&& other) noexcept
	{
		bodies.clear();
		bodies = other.bodies;
		bodyLayerFilter = other.bodyLayerFilter;

		other.bodies.clear();
		other.bodyLayerFilter = nullptr;

		return *this;
	}


	// ----------------
	// BODY MANAGEMENT
	// ----------------
	void insert(RigidBody* body);
	void remove(RigidBody* body);

	// ----------------
	// QUERIES
	// ----------------
	void queryPairs(std::vector<BodyPair>& outPairs) const;

	void queryAABB(const AABB& aabb, const QueryFilterInternal& filter, std::vector<RigidBody*>& outBodies) const;
	void queryRay(const Ray& ray, const QueryFilterInternal& filter, std::vector<RaycastHit>& outBodies) const;

};

#endif // !PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H
