#ifndef ANIMATION_COMPONENTS_H
#define ANIMATION_COMPONENTS_H

#include "core/assetsys.h"
#include <vector>
#include <glm/glm.hpp>

struct C_Animator
{
	Asset* asset;

	// state
	int currentAnimation;
	float animationTime;

	bool isPlaying;
	bool isLooping;
};

#endif //ANIMATION_COMPONENTS_H