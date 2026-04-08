#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/ecs/registry.h"
#include "core/components.h"

void Animation_Update(ECS* ecs, float dt);

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

#endif  // ANIMATION_H