#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/components.h"
#include "core/ecs.h"

// stores TRS values
struct BoneTransform {
	glm::vec3 translation;
	glm::quat rotation;
	glm::vec3 scale;
};

void Animation_Update(AdvEng::ECS* ecs, float dt);

// animation control
void Start(C_AnimatedMesh& animator, const char* name); // by name
void Start(C_AnimatedMesh& animator, int id); // by index
void Stop(C_AnimatedMesh& animator);

// blending control
void Blend(C_AnimatedMesh& animatedMesh, const char* name, float blendDuration);
void Blend(C_AnimatedMesh& animatedMesh, int id, float blendDuration);

// settings
void SetLooping(C_AnimatedMesh& animator, bool looping);

// state checks
bool IsRunning(const C_AnimatedMesh& animator);

// getters
float GetAnimationDuration(const C_AnimatedMesh& animatedMesh, int animationIndex);

// calculations
int find_bone_index(Skin* skin, int target_node_index);
void CalculateWorldMatrices(Asset* asset, int currentNode, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices);
void AnimationInterpolation(Asset* asset, Animation& animation, float animationTime, std::vector<BoneTransform>& pose);
void BlendPoses(const std::vector<BoneTransform>& poseA, const std::vector<BoneTransform>& poseB, float blendFactor, std::vector<BoneTransform>& blendedPose);


#endif  // ANIMATION_H