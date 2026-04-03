#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/components.h"
#include "core/ecs.h"

void Animation_Update(AdvEng::ECS* ecs, float dt);

// animation control
void Start(C_AnimatedMesh& animator, const char* name); // by name
void Start(C_AnimatedMesh& animator, int id); // by index
void Stop(C_AnimatedMesh& animator);

// settings
void SetLooping(C_AnimatedMesh& animator, bool looping);

// state checks
bool IsRunning(const C_AnimatedMesh& animator);
bool WillExpire(const C_AnimatedMesh& animator);

// getters
float GetDuration(const C_AnimatedMesh& animator);
float GetCurrentTime(const C_AnimatedMesh& animator);

// calculations
void CalculateWorldMatrices(Asset* asset, int boneIndex, const std::vector<int>& nodeIndices, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices);

#endif  // ANIMATION_H