#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/ecs/registry.h"
#include "core/animation/components.h"

void Animation_Update(AdvEng::ECS* ecs, float dt);

// animation control
void Start(C_Animator& animator, const char* name); // by name
void Start(C_Animator& animator, int id); // by index
void Stop(C_Animator& animator);

// settings
void SetLooping(C_Animator& animator, bool looping);

// state checks
bool IsRunning(const C_Animator& animator);
bool WillExpire(const C_Animator& animator);

// getters
float GetDuration(const C_Animator& animator);
float GetCurrentTime(const C_Animator& animator);

// calculations
void CalculateWorldMatrices(Asset* asset, int boneIndex, const std::vector<int>& nodeIndices, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices);

#endif  // ANIMATION_H