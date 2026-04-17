#include "foundations/scene.h"
#include "foundations/components.h"
#include "renderer/renderer.h"

// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "imported_components.h"

// types from physics
#include "physics/core/types.h"

#include "SDL3/SDL.h"

namespace rj = rapidjson;

void Scene::StartUp()
{
    // NOT NEEDED, AddComponent uses GetComponentPoolPtr, ...
    // ... which registers the component
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_StaticMesh>();

    m_asset = NULL;
}

void Scene::Shutdown()
{
    // for now
    free_asset(m_asset);
}

bool Scene::LoadAsset(const char* fileName) 
{
    /*if (m_asset)
    {
        free_asset(m_asset);
    }*/
    Asset* asset = load_asset(fileName);

    // Load the nodes
    for (size_t i = 0; i < asset->node_count; i++)
    {
        Node* node = &asset->nodes[i];
        
        // RapidJSON to ECS tutorial!!1!1!!!!!1
        // 1. We parse extras_json with rapidjson::Document
        rj::Document doc;
        if (node->extras_json)
        {
            doc.Parse(node->extras_json);
        }
        else
        {
            continue;
        }
        
        // 2. And put the "_ecs" value in the following
        if (!doc.HasMember("_ecs")) continue;
        
        rj::Value& components = doc["_ecs"];
        
        // 2-extra.
        // We will probably need to add a single flag in _ecs, like "isEntity" or "EntityComponent" idk, 
        // saying if it is an entity, to create it (bones are not going to be i think)
        // if (components.HasMember("isEntity") && (components.GetBool() == true)) {}

        EntityID eID;
        eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");

        // 3. For ImportedComponents that use mirrored data from the json,
        // Check if it cointains the member "____Component" and fill it using Struct
        // ---------------
        // -- TRANSFORM --
        // ---------------
        C_Transform t;
        t.position = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        t.rotation = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
        t.matrix = glm::mat4_cast(t.rotation);
        t.matrix = glm::translate(t.matrix, t.position);
        m_ecs.AddComponent<C_Transform>(eID, { t.position, t.rotation, t.matrix });


        // -------------------
        // RIGIDBODY COMPONENT
        // -------------------
        if (components.HasMember("RigidbodyComponent"))
        {
            // 3.1. Automated import!!!! you don't have to do anything just declare it!!!!
            ImportedRigidbody importedRigidbody = StructFromRapidJsonValue<ImportedRigidbody>(components["RigidbodyComponent"]);

            bool supported = true;
            float halfHeight;
            // 3.2. use the ImportedComponent as a helper for your C_Component
            ShapeDesc shapeDesc;
            switch (importedRigidbody.collider_type)
            {
            case ImportedColliderType::COL_TYPE_BOX:
                shapeDesc = ShapeDesc::makeBox(importedRigidbody.half_widths);
                break;
            case ImportedColliderType::COL_TYPE_SPHERE:
                shapeDesc = ShapeDesc::makeSphere(importedRigidbody.radius);
                break;
            case ImportedColliderType::COL_TYPE_CAPSULE:
                // convert importedRigidbody.height (full height with radii) to halfHeight (center to sphere)
                halfHeight = std::max(0.0f, importedRigidbody.height / 2 - importedRigidbody.radius);
                shapeDesc = ShapeDesc::makeCapsule(importedRigidbody.radius, halfHeight);
                break;
            default:
                supported = false;
                SDL_assert(false && "Shape Type not supported for now");
                break;
            }

            if (!supported) continue; // ignore current entity if has unsupported shape type

            shapeDesc.localOffset = importedRigidbody.collider_position_offset;
            shapeDesc.localOrientation = importedRigidbody.collider_rotation_offset;

            // Create the shape
            ShapeHandle shapeHandle = m_physicsManager.createShape(shapeDesc);

            RigidBodyDesc rbDesc;

            rbDesc.position = t.position;
            rbDesc.orientation = t.rotation;
            rbDesc.mass = importedRigidbody.mass;
            rbDesc.gravityScale = importedRigidbody.gravity_scale;
            rbDesc.damping = importedRigidbody.damping;
            rbDesc.forceLayers = importedRigidbody.force_layers;
            rbDesc.isStatic = importedRigidbody.is_static;
            rbDesc.isKinematic = importedRigidbody.is_kinematic;
            rbDesc.isCharacter = importedRigidbody.is_character;
            rbDesc.isTrigger = importedRigidbody.is_trigger;

            rbDesc.shape = shapeHandle;

            RigidBodyHandle rbHandle = m_physicsManager.createBody(eID, rbDesc);

            // 3.3 If we had more data to acces (in the node), feel free to add data to your component, 
            // but that won't probably be the case for player defined components

            // 3.4 Finally add the component to the ECS!!!
            /*m_ecs.AddComponent<C_Collider>(eID, std::move(colliderComponent));*/
            m_ecs.AddComponent<C_RigidBody>(eID, { rbHandle });
        }
        // 4. END OF RAPIDJSON EXAMPLE AND OUR REFLECTION SYSTEM !!!

        // -- MESH
        if (node->mesh_index >= 0)
        {
            // printf("Adding mesh!\n");
            C_StaticMesh staticMesh{
                &asset->meshes[node->mesh_index],
                asset
            };
            m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });
        }
    }

    m_asset = asset;

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
    bool success = LoadAsset(fileName);
    if (!success) return false;

    // NOTE: Jaime's view returns a sparse set, aka, some C++ iterator.
    // The renderer takes a contiguous array of data.
    // Therefore, get the view, and iterate over it to build the contiguous array to init the scene.
    auto packed = m_ecs.GetView<C_StaticMesh>().GetPacked();
    
    std::vector<C_StaticMesh*> meshes;
    for (size_t i = 0; i < packed.size(); ++i)
    {
        auto& [static_mesh] = packed[i].components;
        meshes.push_back(&static_mesh);
    }

/* TEMP ANIMATION STUFF (moved Pio's test code here for now before animation is loaded properly) */
    // Animation test
    Asset* asset3 = load_asset("assets/animations/Animationtest.gltf");
    // Asset* asset3 = load_asset("assets/animations/zombie.gltf");
    SDL_Log("Asset 3 Extras: %s\n", asset3->nodes[0].extras_json);

    // Find how many joints the zombie has
    uint32_t zombie_joint_count = 0;
    if (asset3->skin_count > 0) {
        zombie_joint_count = asset3->skins[0].joint_count;
    }
    else {
        zombie_joint_count = 1; 
    }

    C_AnimatedMesh temp_animated_mesh = {
        .mesh = &asset3->meshes[3],
        .asset = asset3,
        .joint_count = zombie_joint_count,
        .joint_matrices = (glm::mat4*)malloc(zombie_joint_count * sizeof(glm::mat4))
    };

    for (uint32_t j = 0; j < zombie_joint_count; j++) {
        temp_animated_mesh.joint_matrices[j] = glm::mat4(1.0f);
    }

    // TEMP HACK BCUZ MESHES NOT IN THE LOADER YET
    C_AnimatedMesh* anim_component;
    {
        EntityID eID;
        eID = m_ecs.CreateEntity("Temp animated entity");
        Node* node = &asset3->nodes[0];
        C_Transform t;
        t.position = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        t.rotation = glm::quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
        t.matrix = glm::mat4_cast(t.rotation);
        t.matrix = glm::translate(t.matrix, t.position);
        m_ecs.AddComponent<C_Transform>(eID, { t.position, t.rotation, t.matrix });


        m_ecs.AddComponent<C_AnimatedMesh>(eID, std::move(temp_animated_mesh));

        anim_component = &m_ecs.GetComponent<C_AnimatedMesh>(eID);
    }

    /* END TEMP ANIMATION STUFF */

    Scene_InitInfo info = {};
    info.num_static_meshes = (uint32_t)meshes.size();
    info.static_meshes = meshes.data();
    info.num_animated_meshes = 1;            // <- TEMP
    info.animated_meshes = &anim_component;  // <- TEMP
    

    Renderer_ChangeScene(info);
    
    return true;
}

void Scene::Update(float dt)
{
    m_physicsManager.update(m_ecs, dt);
}

void Scene::Render()
{
    m_ecs.GetView<C_Transform, C_StaticMesh>().ForEach([&](C_Transform& transform, C_StaticMesh& mesh)
    {
        // Skip invalid / not-yet-uploaded meshes
        if (mesh.renderer_prefab.mesh_rids.primitive_count == 0)
            return;

        Renderable r{};
        r.transform = transform.matrix;
        r.mesh_prefab = mesh.renderer_prefab;
        r.joint_count = 0;
        r.joints = nullptr;

        Renderer_PushRenderable(r);
    });

    m_ecs.GetView<C_Transform, C_AnimatedMesh>().ForEach([&](C_Transform& transform, C_AnimatedMesh& mesh)
    {
        // Skip invalid / not-yet-uploaded meshes
        if (mesh.renderer_prefab.mesh_rids.primitive_count == 0)
            return;
        
        Renderable r{};
        r.transform = transform.matrix;
        r.mesh_prefab = mesh.renderer_prefab;
        r.joint_count = mesh.joint_count;
        r.joints = mesh.joint_matrices;

        Renderer_PushRenderable(r);
    });
}
