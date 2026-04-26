#ifndef PHYSICS_QUERIES_QUERY_FILTER_H
#define PHYSICS_QUERIES_QUERY_FILTER_H

#include "physics/core/types.h"

struct QueryFilterInternal
{

};

struct QueryFilter
{
	RigidBodyHandle bodyToIgnore;
	// TODO: add the body layers (broadphase, like jolt) (no need to change the current data from the script)
	
};


#endif // !PHYSICS_QUERIES_QUERY_FILTER_H

