#ifndef DEBUG_UI_API_H
#define DEBUG_UI_API_H

#include "core/ecs.h"
#include "core/assetsys.h"

namespace DebugUI
{
    void SetECS(AdvEng::ECS* ecs);
    void SetAsset(Asset* asset);
};

#endif  // DEBUG_UI_API_H
