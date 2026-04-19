#include "broadphase.h"

BroadPhase::BroadPhase()
{

}

// Acceleration structure not found :sob:
//BroadPhase::BroadPhase(float cellSize, glm::vec3 gridWorldOrigin)
//{
//}

void BroadPhase::insert(RigidBody* body)
{
	bool found = false;
	size_t idx = bodies.size();
	for (size_t i = 0; i < bodies.size() && !found; i++)
	{
		if (bodies[i]->bodyID == body->bodyID)
		{
			idx = i;
			found = true;
		}
	}

	if (idx < bodies.size())
	{
		SDL_assert(false && "Broadphase: Rigidbody already inserted, erase first");
		return;
	}

	bodies.push_back(body);
}

void BroadPhase::remove(RigidBody* body)
{
	bool found = false;
	size_t idx = bodies.size();
	for (size_t i=0; i < bodies.size() && !found; i++)
	{
		if (bodies[i]->bodyID == body->bodyID)
		{
			idx = i;
			found = true;
		}
	}

	if (!found)
	{
		SDL_assert(false && "Broadphase: Removing body that is not inserted");
		return;
	}

	bodies[idx] = bodies.back();
	bodies.pop_back();
}

void BroadPhase::queryPairs(std::vector<BodyPair>& outPairs) const
{
	// O(n^2) for now. until acceleration structure done
	for (size_t i = 0; i < bodies.size(); i++)
	{
		for (size_t j = i+1; j < bodies.size(); j++)
		{
			RigidBody* a = bodies[i];
			RigidBody* b = bodies[j];

			if (a->isStatic && b->isStatic) continue;
			if (a->isTrigger && b->isTrigger) continue;

			if ((a->position.y < 1.5f && b->position.y == 0.0f)
				|| (b->position.y < 1.5f && a->position.y == 0.0f))
			{
				SDL_assert(true);
			}

			if (a->aabb.overlaps(b->aabb))
				outPairs.push_back({ a, b });
		}
	}
}

void BroadPhase::queryAABB(const AABB& aabb, const QueryFilter& filter, std::vector<RigidBody*> outBodies) const
{
}

void BroadPhase::queryRay(const Ray& ray, const QueryFilter& filter, std::vector<RigidBody*> outBodies) const
{
}