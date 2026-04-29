#include "core/ecs.h"
#include "core/my_c_runtime.h"
#include "core/assetsys.h"

#include "level/hierarchy.h"

#include "physics/physics_manager.h"


class Scene {

private:
    ECS m_ecs;
    PhysicsManager m_physicsManager;

    std::vector<Asset*> m_prefabs;

    Hierarchy<u32> m_hierarchy;

private:
    //bool LoadAsset(const char* fileName);
    bool FreeAsset(Asset* asset);
    

public:
    Scene() {}
    ~Scene() {}

    void StartUp();
    void Shutdown();

    // loads asset and stores it in m_prefabs
    Asset* LoadPrefab(const char* fileName);
    // converts prefab into an entity with a given position
    // return ID of first node
    EntityID InstantiatePrefab(Asset* prefab, glm::vec3 position);
    // collects all renderables and initializes the renderer scene
    void BuildRendererScene();

    ECS& GetECS() { return m_ecs; };

    void Update(float dt);
    void Render();

};

