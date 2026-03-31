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
    return;
}

void Start(C_Animator& animator, const char* name) {return;} // by name
void Start(C_Animator& animator, int id) { return; } // by index
void Stop(C_Animator& animator) { return; }

// settings
void SetLooping(C_Animator& animator, bool looping) { return; }

// state checks
bool IsRunning(const C_Animator& animator) { return true; }
bool WillExpire(const C_Animator& animator) { return true; }

// getters
float GetDuration(const C_Animator& animator) { return 0.0f; }
float GetCurrentTime(const C_Animator& animator) { return 0.0f; }
