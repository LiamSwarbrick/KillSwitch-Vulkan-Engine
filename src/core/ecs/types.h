#ifndef ECS_TYPES_H
#define ECS_TYPES_H

#include "core/my_c_runtime.h"

#include <bitset>
#include <limits>

namespace AdvEng
{
	using EntityID = u32;

	static constexpr EntityID NULL_ENTITY = std::numeric_limits<EntityID>::max();
	constexpr EntityID MAX_ENTITIES = NULL_ENTITY;
	static constexpr EntityID NULL_ID = std::numeric_limits<EntityID>::max();

	constexpr size_t MAX_COMPONENTS = 32;
	using ComponentMask = std::bitset<MAX_COMPONENTS>;

	// v2, added pagination to sparse set
	static constexpr size_t PAGE_SIZE = 1024;
	using SparsePage = std::array<EntityID, PAGE_SIZE>;
}

#endif // !ECS_TYPES_H