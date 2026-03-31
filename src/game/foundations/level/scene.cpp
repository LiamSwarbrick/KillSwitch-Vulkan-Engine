#include "foundations/scene.h"
#include "foundations/components.h"

// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "level/imported_components.h"

#include "SDL3/SDL.h"

namespace rj = rapidjson;

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

        // RapidJSON to ECS tutorial!!1!1!!!!!1
        // 1. We parse extras_json with rapidjson::Document
        rj::Document doc;
        doc.Parse(node->extras_json);

        // 2. And put the "_ecs" value in the following
        rj::Value& components = doc["_ecs"];
        
        // 2-extra.
        // We will probably need to add a single flag in _ecs, like "isEntity" or "EntityComponent" idk, 
        // saying if it is an entity, to create it (bones are not going to be i think)
        // if (components.HasMember("isEntity") && (components.GetBool() == true)) {}

        AdvEng::EntityID eID;
        eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");

        // 3. For ImportedComponents that use mirrored data from the json,
        // Check if it cointains the member "____Component" and fill it using Struct
        // ---------------
        // -- COLLIDER ---
        // ---------------
        if (components.HasMember("ColliderComponent"))
        {
            // 3.1. Automated import!!!! you don't have to do anything just declare it!!!!
            ImportedCollider importedCollider = StructFromRapidJsonValue<ImportedCollider>(components["ColliderComponent"]);

            // 3.2. use the ImportedComponent as a helper for your C_Component
            C_Collider colliderComponent;
            switch (importedCollider.collider_type)
            {
            case ImportedColliderType::BOX:
                colliderComponent.type = ColliderType::Box;
                colliderComponent.box.halfWidths = importedCollider.half_widths;
                break;
            case ImportedColliderType::SPHERE:
                colliderComponent.type = ColliderType::Sphere;
                colliderComponent.sphere.radius = importedCollider.radius;
                break;
            case ImportedColliderType::CAPSULE:
                colliderComponent.type = ColliderType::Capsule;
                colliderComponent.capsule.radius = importedCollider.radius;
                colliderComponent.capsule.height = importedCollider.height;
                break;
            default:
                // what the helly (sorry im tired)
                SDL_assert(false);
                break;
            }

            // 3.3 If we had more data to acces (in the node), feel free to add data to your component, 
            // but that won't probably be the case for player defined components

            // 3.4 Finally add the component to the ECS!!!
            m_ecs.AddComponent<C_Collider>(eID, std::move(colliderComponent));
        }
        // 4. END OF RAPIDJSON EXAMPLE AND OUR REFLECTION SYSTEM !!!


        // ---------------
        // -- TRANSFORM --
        // ---------------
        C_Transform t;
        t.position = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        t.rotation = glm::quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
        t.matrix = glm::mat4_cast(t.rotation);
        t.matrix = glm::translate(t.matrix, t.position);
        m_ecs.AddComponent<C_Transform>(eID, { t.position, t.rotation, t.matrix });

        C_StaticMesh staticMesh{
           &asset->meshes[node->mesh_index],
           asset
        };
        m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });

        

        
        
        
        
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