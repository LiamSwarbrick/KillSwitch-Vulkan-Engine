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
	std::vector<BodyPair> queryPairs() const;

	std::vector<RigidBody*> queryAABB() const;
};

#endif // !PHYSICS_COLLISION_BROADPHASE_BROADPHASE_H
