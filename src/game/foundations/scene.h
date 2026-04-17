#include "core/ecs.h"
#include "core/my_c_runtime.h"
#include "core/assetsys.h"

#include "level/hierarchy.h"

#include "physics/physics_manager.h"


class Scene {

private:
    ECS m_ecs;
    PhysicsManager m_physicsManager;

    // For now
    Asset* m_asset;

    // Not usable for now
    Hierarchy<u32> m_hierarchy;

private:
    bool LoadAsset(const char* fileName);
    bool FreeAsset(Asset* asset);

public:
    Scene() {}
    ~Scene() {}

    void StartUp();
    void Shutdown();

    // bool LoadLevel(u32 levelNumber);
    bool LoadLevel(const char* fileName);
    ECS& GetECS()   { return m_ecs;    };
    Asset*        GetAsset() { return m_asset;  };

    void Update(float dt);
    void Render();

};

