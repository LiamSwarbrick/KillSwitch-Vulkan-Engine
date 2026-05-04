#ifndef FOUNDATIONS_BODY_LAYER_COLLISIONS_H

#include <cstdint>

// Important to set this (could do macros but im tired of them)
const uint8_t NUM_BODY_LAYERS = 6;

enum class BodyLayer : uint8_t
{
	// The first block of layers are for bodies.bodyLayer
	STATIC = 0U,
	MOVING,
	CHARACTER, // Player & zombies, basically hittables by attacks, if later want attacks 

	// This next block is for queries, you set them to affect the previous layers so you can get an optimized query and skip many other objects
	WEAPON, // Affect MOVING & CHARACTER
	AFFECT_ONLY_CHARACTER,
	AFFECT_ONLY_STATIC,
};

#endif // !FOUNDATIONS_BODY_LAYER_COLLISIONS_H

