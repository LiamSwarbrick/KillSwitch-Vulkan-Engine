#ifndef PHYSICS_COLLISION_BROADPHASE_GRID_H
#define PHYSICS_COLLISION_BROADPHASE_GRID_H


#include "physics/core/types.h"

#include <vector>

struct Ray;
struct QueryFilter;

class Grid
{
public:
	explicit Grid(float cellSize = 5.0f);

	void clear();

	void insert(RigidBody* body, const AABB& aabb);
	void remove(RigidBody* body, const AABB& aabb);

	void queryPairs(std::vector<BodyPair>& outPairs) const;

	// phase 1 of 
	void queryAABB(const AABB& aabb, const QueryFilter& filter, std::vector<RigidBody*>& outBodies);

	void queryRay(const Ray& ray, const QueryFilter& filter, std::vector<RigidBody*>& outBodies);

	void setCellSize(float size);
	float getCellSize() const;
	
	// Would be ideal to be able to debug render like
	// void debugDraw();
private:
	float cellSize;

	struct Cell
	{
		std::vector<RigidBody*> bodies;
	};

	struct CellCoord
	{
		int x, y, z;
		
		bool operator==(const CellCoord& o) const
		{
			return x == o.x && y == o.y && z == o.z;
		}
	};

	struct CellCoordHash
	{
		size_t operator()(const CellCoord& c) const
		{
			// divide the coordinate by the cell size and then hash that coordinate
			// add the coordinates together to form a hash or something
		}
	};
};

#endif // !PHYSICS_COLLISION_BROADPHASE_GRID_H
