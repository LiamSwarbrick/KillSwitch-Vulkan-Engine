#ifndef FOUNDATIONS_SCENE_H
#define FOUNDATIONS_SCENE_H

#include "core/ecs.h"
#include "core/my_c_runtime.h"
#include "core/assetsys.h"

#include "level/hierarchy.h"

#include "physics/physics_manager.h"


class Scene {

private:
    ECS m_ecs;
    PhysicsManager m_physicsManager;

    EntityID m_currentPlayer = NULL_ENTITY;

    Subscription<CollisionEnterAndStayArgs> m_onCollisionEnterSubscription;
    Subscription<CollisionEnterAndStayArgs> m_onCollisionStaySubscription;

    Hierarchy<u32> m_hierarchy;

    glm::vec3 m_movementCameraForward = glm::vec3(0.0f, 0.0f, -1.0f);

public:
    std::vector<Asset*> m_prefabs;
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
    EntityID InstantiatePrefab(Asset* prefab, glm::vec3 spawnPosition, glm::quat spawnRotation);
    // collects all renderables and initializes the renderer scene
    void BuildRendererScene();

    ECS& GetECS() { return m_ecs; };
    PhysicsManager& GetPhysicsManager() { return m_physicsManager; }

    void Update(float dt);
    void Render();

    void SetPlayer(EntityID id) { m_currentPlayer = id; }
    void SetMovementCameraForward(const glm::vec3& forward) { m_movementCameraForward = forward; }

private:
    void UpdatePlayer(float dt);

private:
    // Helper to set the body's collision matrix
    void SetBodyCollisionLayers();
};

#endif //FOUNDATIONS_SCENE_H