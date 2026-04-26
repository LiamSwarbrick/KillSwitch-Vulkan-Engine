#ifndef CORE_COMPONENTS_H
#define CORE_COMPONENTS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "renderer/render_types.h"
#include "core/assetsys.h"

#include <vector>

struct C_Transform
{
    glm::mat4 matrix;
};

typedef enum LightComponentType
{
	LIGHT_COMPONENT_POINTLIGHT,
	LIGHT_COMPONENT_SPOTLIGHT
}
LightComponentType;

// NOTE:
// The direction of the spotlight is along the local Z axis
// The light component's parent entity defines it transform.
struct C_Light
{
	LightComponentType type;
	glm::vec3 color;
	float intensity;
	float radius;
	float spot_inner_cone_angle;  // <- Spotlights only
	float spot_outer_cone_angle;
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
	char* idleAnimationName;

    // NEW LAYER STRUCTS
    AnimationLayer lowerBodyLayer;
	AnimationLayer upperBodyLayer;

    // layer data
	char* splitJointName;
	bool isUpperLayerActive = false;
	double upperBodyLayerWeight;
    std::vector<float> boneMask;
	float layerBlendDuration;
	int layerBlendDirection; // 1 for blending to upper body, -1 for blending back to lower body

	// aiming data
	bool isAiming;
	float aimYaw;
	float aimPitch;
	int spineIndices[3];

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