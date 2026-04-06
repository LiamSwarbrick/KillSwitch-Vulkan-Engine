#include "foundations/scene.h"
#include "foundations/components.h"
#include "renderer/renderer.h"

// animation update
#include "core/animation.h"
// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "imported_components.h"

#include "SDL3/SDL.h"

namespace rj = rapidjson;

void Scene::StartUp()
{
    // NOT NEEDED, AddComponent uses GetComponentPoolPtr, ...
    // ... which registers the component
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_StaticMesh>();
    m_ecs.RegisterComponent<C_AnimatedMesh>();

    m_asset = NULL;
}

void Scene::Shutdown()
{
    // for now
    free_asset(m_asset);
}

bool Scene::LoadAsset(const char* fileName) 
{
    if (m_asset)
    {
        free_asset(m_asset);
    }
    Asset* asset = load_asset(fileName);

    // Load the nodes
    for (size_t i = 0; i < asset->node_count; i++)
    {
        Node* node = &asset->nodes[i];
        
        // RapidJSON to ECS tutorial!!1!1!!!!!1
        // 1. We parse extras_json with rapidjson::Document
        rj::Document doc;
        bool has_ecs_data = false;

        if (node->extras_json)
        {
            doc.Parse(node->extras_json);
            if (doc.HasMember("_ecs")) {
                has_ecs_data = true;
            }
        }

        AdvEng::EntityID eID;
        eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
        
        if (has_ecs_data)
        {
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

        // -- MESH
        if (node->mesh_index >= 0)
        {
            Mesh* mesh = &asset->meshes[node->mesh_index];
            if (mesh->vertex_type == VERTEX_TYPE_SKINNED)
            {
                uint32_t joint_count = 0;
                if (node->skin_index >= 0) {
                    joint_count = (uint32_t)asset->skins[node->skin_index].joint_count;
                }
                else if (asset->skin_count > 0) {
                    joint_count = (uint32_t)asset->skins[0].joint_count;
                }
                else {
                    joint_count = 1;
                }

                C_AnimatedMesh animMesh
                {
                    mesh,
                    asset
                };
                animMesh.joint_count = joint_count;
                animMesh.currentAnimation = 0;
                animMesh.animationTime = 0.0f;
                animMesh.isPlaying = true;
                animMesh.isLooping = true;

                if (joint_count > 0) {
                    animMesh.joint_matrices = (glm::mat4*)malloc(joint_count * sizeof(glm::mat4));
                    for (uint32_t j = 0; j < joint_count; j++) {
                        animMesh.joint_matrices[j] = glm::mat4(1.0f);
                    }
                }
                else {
                    animMesh.joint_matrices = nullptr;
                }

                m_ecs.AddComponent<C_AnimatedMesh>(eID, std::move(animMesh));
            }
            else
            {
                C_StaticMesh staticMesh{
                    &asset->meshes[node->mesh_index],
                    asset
                };
                m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });
            }
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

    auto packed_anim = m_ecs.GetView<C_AnimatedMesh>().GetPacked();

    std::vector<C_AnimatedMesh*> anim_meshes;
    for (size_t i = 0; i < packed_anim.size(); ++i)
    {
        auto& [anim_mesh] = packed_anim[i].components;
        anim_meshes.push_back(&anim_mesh);
    }

    Scene_InitInfo info = {};
    info.num_static_meshes = (uint32_t)meshes.size();
    info.static_meshes = meshes.data();
    info.num_animated_meshes = (uint32_t)anim_meshes.size();
    info.animated_meshes = anim_meshes.data();

    Renderer_ChangeScene(info);
    
    return true;
}

void Scene::Update(float dt)
{
    Animation_Update(&m_ecs, dt);
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
