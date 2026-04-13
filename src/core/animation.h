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
void OnStartAnim(C_AnimatedMesh& animatedMesh, const char* animationName); // Sets initial lower body animation with no blending
void PlayAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration); // Blends from the current lower body animation to the given one
void PlayUpperBodyAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration); // Blends from either lower body or current upper body to the given one
void StopAnim(C_AnimatedMesh& animatedMesh, float blendDuration); // Just blends from current lower body to idle animation
void StopUpperBodyAnim(C_AnimatedMesh& animatedMesh, float blendDuration); // Blends from current upper body to lower body
void PlayFullBodyAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration); // Stops the current upper body animation and blends from lower body to the given one

// settings
void SetLooping(C_AnimatedMesh& animator, AnimationLayer& layer, bool looping);

// state checks
bool IsRunning(const C_AnimatedMesh& animator, const AnimationLayer& layer);

// getters
float GetAnimationDuration(const C_AnimatedMesh& animatedMesh, int animationIndex);
int GetAnimationIdFromName(const C_AnimatedMesh& animatedMesh, const char* animationName);

// calculations
int find_bone_index(Skin* skin, int target_node_index);
void UpdateLayerTime(C_AnimatedMesh& animatedMesh, AnimationLayer& layer, float dt, float playbackSpeed);
void CalculateLayerPose(Asset* asset, AnimationLayer& layer, std::vector<BoneTransform>& currentPose);
void CalculateWorldMatrices(Asset* asset, int currentNode, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices);
void AnimationInterpolation(Asset* asset, Animation& animation, float animationTime, std::vector<BoneTransform>& pose);
void BlendPoses(const std::vector<BoneTransform>& poseA, const std::vector<BoneTransform>& poseB, float blendFactor, std::vector<BoneTransform>& blendedPose);

// layered animation
void SetBoneMask(C_AnimatedMesh& animatedMesh, int boneIndex);
void CreateUpperBodyLayer(C_AnimatedMesh& animatedMesh, const char* splitJointName);

// lookat control
void FindUpperBodyBones(C_AnimatedMesh& animatedMesh);
void SetAimingRotations(C_AnimatedMesh& animatedMesh, std::vector<BoneTransform>& pose, float yaw, float pitch);



#endif  // ANIMATION_H