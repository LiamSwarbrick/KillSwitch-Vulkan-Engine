#ifndef ECS_TYPES_H
#define ECS_TYPES_H


#include <bitset>
#include <limits>
#include <cstdint>


using EntityID = uint32_t;

static constexpr EntityID NULL_ENTITY = std::numeric_limits<EntityID>::max();
// fill with custom value for maximum number of entities (defaults to U32_MAX)
constexpr EntityID MAX_ENTITIES = NULL_ENTITY;

static constexpr EntityID NULL_ID = std::numeric_limits<EntityID>::max();

#warning ECS:: Maximum number of components is 32, please don't forget to adjust accordingly in ecs/types.h
constexpr size_t MAX_COMPONENTS = 32;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// v2, added pagination to sparse set
static constexpr size_t PAGE_SIZE = 1024;
using SparsePage = std::array<EntityID, PAGE_SIZE>;


#endif // !ECS_TYPES_H