#ifndef FOUNDATIONS_BODY_LAYER_COLLISIONS_H

#include <cstdint>

const uint8_t NUM_BODY_LAYERS = 3;

enum class BodyLayer : uint8_t
{
	STATIC = 0U,
	MOVING,
	WEAPON,
};

#endif // !FOUNDATIONS_BODY_LAYER_COLLISIONS_H

