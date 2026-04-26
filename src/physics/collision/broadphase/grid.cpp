#include "grid.h"

#include "physics/queries/raycast.h"
#include "physics/queries/query_filter.h"

Grid::Grid(float cellSize)
{
}

void Grid::insert(RigidBody* body, const AABB& aabb)
{
}

void Grid::remove(RigidBody* body, const AABB& aabb)
{
}

void Grid::queryAABB(const AABB& aabb, const QueryFilter& filter, std::vector<RigidBody*>& outBodies)
{
}

void Grid::queryRay(const Ray& ray, const QueryFilter& filter, std::vector<RigidBody*>& outBodies)
{
}

void Grid::setCellSize(float size)
{
}

float Grid::getCellSize() const
{
	return 0.0f;
}
