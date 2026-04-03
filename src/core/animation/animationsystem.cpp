#include "animation.h"
#include "components.h"

void Animation_Update(AdvEng::ECS* ecs, float dt)
{
    //auto view = ecs->GetView<C_Animator>();

    //for (AdvEng::EntityID e : view)
    //{
    //    auto& animator = ecs->GetComponent<C_Animator>(e);
    //    
    //}

	// Compute final matrices and update animation time for each animator
    return;
}

void Start(C_AnimatedMesh& animator, const char* name) {return;} // by name
void Start(C_AnimatedMesh& animator, int id) { return; } // by index
void Stop(C_AnimatedMesh& animator) { return; }

// settings
void SetLooping(C_AnimatedMesh& animator, bool looping) { return; }

// state checks
bool IsRunning(const C_AnimatedMesh& animator) { return true; }
bool WillExpire(const C_AnimatedMesh& animator) { return true; }

// getters
float GetDuration(const C_AnimatedMesh& animator) { return 0.0f; }
float GetCurrentTime(const C_AnimatedMesh& animator) { return 0.0f; }
