#include "broadphase.h"

BroadPhase::BroadPhase(BodyLayerFilter* bodyLayerFilter)
	: bodyLayerFilter(bodyLayerFilter)
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

			// Not including isTrigger here cause we can spawn a trigger that is going to apply a force on a sleeping body 
			// and that SHOULD BE A CONTACT
			bool skipA = a->isStatic || a->sleeping;
			bool skipB = b->isStatic || b->sleeping;
			if (skipA && skipB) continue;
			if (a->isTrigger && b->isTrigger) continue;
			if (!bodyLayerFilter->shouldCollide(a->bodyLayer, b->bodyLayer)) continue;


			if (a->aabb.overlaps(b->aabb))
				outPairs.push_back({ a, b });
		}
	}
}

void BroadPhase::queryAABB(const AABB& aabb, const QueryFilterInternal& filter, std::vector<RigidBody*>& outBodies) const
{
	for (RigidBody* body : bodies)
	{
		if (filter.bodyToIgnore && filter.bodyToIgnore == body) continue;
		if (filter.hasLayerOfQuery && !bodyLayerFilter->shouldCollide(filter.layerOfQuery, body->bodyLayer)) continue;

		if (aabb.overlaps(body->aabb))
		{
			outBodies.push_back(body);
		}
	}
}

void BroadPhase::queryRay(const Ray& ray, const QueryFilterInternal& filter, std::vector<RaycastHit>& outBodies) const
{
	for (RigidBody* body : bodies)
	{
		RaycastHit hit;
		bool shouldCollide = filter.hasLayerOfQuery ? bodyLayerFilter->shouldCollide(filter.layerOfQuery, body->bodyLayer) : true;

		if (filter.bodyToIgnore && filter.bodyToIgnore == body) continue;
		if (!shouldCollide) continue;
		
		if (body->aabb.intersectsRay(ray, hit) || body->aabb.contains(ray.origin))
		{
			hit.body = body;
			outBodies.push_back(hit);
		}
	}
}