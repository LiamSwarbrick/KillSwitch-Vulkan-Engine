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

	// renderer reads this to get the final joint matrices for skinning
	std::vector<glm::mat4> final_joint_matrices;
};

#endif //ANIMATION_COMPONENTS_H