#include "ZombieAISystem.h"

void ZombieAISystem::Update(float dt) const
{
    auto view = ecs->GetView<C_Transform, C_ZombieAIInfo, C_MovementInput, C_CombatInput>();
    view.ForEach([&](EntityID entity, C_Transform& transform, C_ZombieAIInfo& info, C_MovementInput& moveInput, C_CombatInput& combatInput)
    {
        // READ FROM: ZombieAIInfo and PhysicsManager
        // WRITE TO: ZombieAIInfo, MovementInput, CombatInput
        
            info.previousState = info.currentState;
            info.currentState = info.Idle;

            moveInput.moveAmount = 0.0f;
        

    });
}
