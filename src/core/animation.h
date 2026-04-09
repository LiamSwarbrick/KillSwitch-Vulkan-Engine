#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/components.h"
#include "core/ecs.h"

void Animation_Update(ECS* ecs, float dt);

// animation control
void Start(C_AnimatedMesh& animator, const char* name); // by name
void Start(C_AnimatedMesh& animator, int id); // by index
void Stop(C_AnimatedMesh& animator);

// settings
void SetLooping(C_AnimatedMesh& animator, bool looping);

// state checks
bool IsRunning(const C_AnimatedMesh& animator);

// getters
float GetDuration(const C_AnimatedMesh& animator);

// calculations
int find_bone_index(Skin* skin, int target_node_index);
void CalculateWorldMatrices(Asset* asset, int currentNode, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices);

#endif  // ANIMATION_H