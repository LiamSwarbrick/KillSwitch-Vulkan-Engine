#include "foundations/scene.h"
#include "foundations/components.h"

#include "SDL3/SDL.h"

void Scene::StartUp()
{
    // NOT NEEDED, AddComponent uses GetComponentPoolPtr, ...
    // ... which registers the component
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_StaticMesh>();
}

void Scene::Shutdown()
{
    // for now
    free_asset(m_asset);
}

bool Scene::LoadAsset(const char* fileName) 
{
    Asset* asset = load_asset(fileName);

    // Load the nodes
    for (size_t i = 0; i < asset->node_count; i++)
    {
        Node* node = &asset->nodes[i];

        // Change to check what type of components does the node have
        // via Flags
        // Then have if statements to check and add components

        C_Transform t;
        t.position = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        t.rotation = glm::quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
        t.matrix = glm::mat4_cast(t.rotation);
        t.matrix = glm::translate(t.matrix, t.position);


        // C_StaticMesh staticMesh{
        //     &asset->meshes[node->mesh_index],
        //     asset
        // };

        AdvEng::EntityID eID;
        eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
        m_ecs.AddComponent<C_Transform>(eID, { t.position, t.rotation, t.matrix });
        // m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });
        
    }
    
    FreeAsset(asset);

    return true;
}

bool Scene::FreeAsset(Asset* asset)
{
    free_asset(asset);

    return true;
}

bool Scene::LoadLevel(const char* fileName)
{
    // For now
    return LoadAsset(fileName);
}

void Scene::Update(float dt)
{

}

void Scene::Render()
{

}