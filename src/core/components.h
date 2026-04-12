#ifndef CORE_COMPONENTS_H
#define CORE_COMPONENTS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "renderer/render_types.h"
#include "core/assetsys.h"

struct C_Transform
{
    glm::vec3 position;
    glm::quat rotation;

    glm::mat4 matrix;
};

struct C_StaticMesh
{
	Mesh* mesh;
	Asset* parent_asset;
    MeshPrefab renderer_prefab;  // Loaded by renderer (empty before that)
};


// stores layer animation state
struct AnimationLayer {
	bool isPreviousLooping = false;
	bool isCurrentLooping = false;
	int currentAnimation = -1;
	int previousAnimation = -1;
	float currentAnimationTime = 0.0f;
	float previousAnimationTime = 0.0f;
	float blendTime = 0.0f;
	float blendDuration = 0.0f;
	bool isBlending = false;
};

struct C_AnimatedMesh
{
    Mesh* mesh;             
    Asset* asset;  
    MeshPrefab renderer_prefab;

    // animator states
    bool isPlaying;
    float playbackSpeed;

    // NEW LAYER STRUCTS
    AnimationLayer lowerBodyLayer;
	AnimationLayer upperBodyLayer;

    // layer data
	bool isUpperlayerActive = false;
	float upperBodyLayerWeight = 0.0f;
    std::vector<float> boneMask;
	float layerBlendDuration = 0.0f;
	int layerBlendDirection = 1; // 1 for blending to upper body, -1 for blending back to lower body

	// skeletal animation data
    uint32_t joint_count;
    glm::mat4* joint_matrices;
};

// struct C_Mesh
// {
//     Mesh* data;
//     Asset* parent_asset;
// };

#endif //CORE_COMPONENTS_H