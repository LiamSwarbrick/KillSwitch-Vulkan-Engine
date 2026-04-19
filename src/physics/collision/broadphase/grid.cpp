#include "grid.h"

#include "physics/queries/raycast.h"
#include "physics/queries/query_filter.h"


void Grid::queryAABB(const AABB& aabb, const QueryFilter& filter, std::vector<RigidBody*>& outBodies)
{
}

void Grid::queryRay(const Ray& ray, const QueryFilter& filter, std::vector<RigidBody*>& outBodies)
{
}
