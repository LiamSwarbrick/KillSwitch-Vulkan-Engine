#ifndef PHYSICS_QUERIES_QUERY_FILTER_H
#define PHYSICS_QUERIES_QUERY_FILTER_H

#include "physics/core/types.h"

struct QueryFilter
{
	RigidBodyHandle bodyToIgnore = InvalidRigidBodyHandle;
	bool hasLayerOfQuery = false;
	uint8_t layerOfQuery = 0;
};

struct QueryFilterInternal
{
	const RigidBody* bodyToIgnore = nullptr;
	bool hasLayerOfQuery = false;
	uint8_t layerOfQuery = 0;
};

#endif // !PHYSICS_QUERIES_QUERY_FILTER_H

