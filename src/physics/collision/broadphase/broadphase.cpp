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

}

void BroadPhase::remove(RigidBody* body)
{
}

std::vector<BodyPair> BroadPhase::queryPairs() const
{
}

std::vector<RigidBody*> BroadPhase::queryAABB() const
{
	return std::vector<RigidBody*>();
}
