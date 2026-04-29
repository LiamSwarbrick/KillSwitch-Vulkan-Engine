#include "game/foundations/scene.h"
#include "game/foundations/components.h"
#include "renderer/renderer.h"

// animation update
#include "core/animation.h"
#include "game/foundations/PlayerMovementSystem.h"
// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "imported_components.h"

#include "SDL3/SDL.h"

namespace rj = rapidjson;

void Scene::StartUp()
{
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_StaticMesh>();
    m_ecs.RegisterComponent<C_AnimatedMesh>();
    m_ecs.RegisterComponent<C_PlayerInput>();
    m_ecs.RegisterComponent<C_CharacterController>();

    m_prefabs.clear();
}

void Scene::Shutdown()
{
    for (Asset* asset : m_prefabs)
    {
        free_asset(asset);
    }

    m_prefabs.clear();
}

Asset* Scene::LoadPrefab(const char* fileName)
{
    uint64_t start_time = SDL_GetTicksNS();

    Asset* asset = load_asset(fileName);

    uint64_t end_time = SDL_GetTicksNS();
    float elapsed_ms = (float)(end_time - start_time) / 1000000.0f;

    if (asset)
    {
        SDL_Log("[Disk Time] Loaded prefab '%s' in %.2f ms", fileName, elapsed_ms);
        m_prefabs.push_back(asset);
    }
    return asset;
}

EntityID Scene::InstantiatePrefab(Asset* prefab, glm::vec3 spawnPosition)
{
    if (!prefab) return MAX_ENTITIES; // or any other invalid ID

    uint64_t start_time = SDL_GetTicksNS();

    EntityID rootEntity = MAX_ENTITIES;

    // Load the nodes into the ECS
    for (size_t i = 0; i < prefab->node_count; i++)
    {
        Node* node = &prefab->nodes[i];

        rj::Document doc;
        bool has_ecs_data = false;

        if (node->extras_json)
        {
            doc.Parse(node->extras_json);
            if (doc.HasMember("_ecs")) {
                has_ecs_data = true;
            }
        }

        EntityID eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
        if (i == 0) rootEntity = eID;

        if (has_ecs_data)
        {
            // 2. And put the "_ecs" value in the following
            rj::Value& components = doc["_ecs"];

            if (components.HasMember("ColliderComponent"))
            {
                ImportedCollider importedCollider = StructFromRapidJsonValue<ImportedCollider>(components["ColliderComponent"]);

            // 3.2. use the ImportedComponent as a helper for your C_Component
            C_Collider colliderComponent;
            switch (importedCollider.collider_type)
            {
            case ImportedColliderType::COL_TYPE_BOX:
                colliderComponent.type = ColliderType::Box;
                colliderComponent.box.halfWidths = importedCollider.half_widths;
                break;
            case ImportedColliderType::COL_TYPE_SPHERE:
                colliderComponent.type = ColliderType::Sphere;
                colliderComponent.sphere.radius = importedCollider.radius;
                break;
            case ImportedColliderType::COL_TYPE_CAPSULE:
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


            if (components.HasMember("PlayerInput"))
            {
                m_ecs.AddComponent<C_PlayerInput>(eID, {});
                m_ecs.AddComponent<C_CharacterController>(eID, {});
            }
        }
            // ---------------
            // -- TRANSFORM --
            // ---------------
            C_Transform t;
            glm::vec3 position = glm::vec3(node->translation[0], node->translation[1], node->translation[2]) + spawnPosition;
            glm::quat rotation = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
            t.matrix = glm::mat4_cast(rotation);
            t.matrix = glm::translate(t.matrix, position);
            m_ecs.AddComponent<C_Transform>(eID, { t.matrix });

        // -- MESH
        if (node->mesh_index >= 0)
        {
            Mesh* mesh = &prefab->meshes[node->mesh_index];
            if (mesh->vertex_type == VERTEX_TYPE_SKINNED)
            {
                uint32_t joint_count = 0;
                if (node->skin_index >= 0) {
                    joint_count = (uint32_t)prefab->skins[node->skin_index].joint_count;
                }
                else if (prefab->skin_count > 0) {
                    joint_count = (uint32_t)prefab->skins[0].joint_count;
                }
                else {
                    joint_count = 1;
                }

                C_AnimatedMesh animMesh{ mesh, prefab };
                animMesh.joint_count = joint_count;
				animMesh.idleAnimationName = "IDLE";
                animMesh.splitJointName = "SPINE";
				OnStartAnim(animMesh, animMesh.idleAnimationName); // Start with idle animation by default
                

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
                C_StaticMesh staticMesh{ &prefab->meshes[node->mesh_index], prefab };
                m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });
            }
        }
    }

    uint64_t end_time = SDL_GetTicksNS();
    float elapsed_ms = (float)(end_time - start_time) / 1000000.0f;
    SDL_Log("[Entity Time] Instantiated %zu nodes into ECS in %.2f ms", prefab->node_count, elapsed_ms);


    return rootEntity;
}

bool Scene::FreeAsset(Asset* asset)
{
    free_asset(asset);

    return true;
}

void Scene::BuildRendererScene()
{
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

    uint64_t after_entity_load = SDL_GetPerformanceCounter();

    Scene_InitInfo info = {};
    info.num_static_meshes = (uint32_t)meshes.size();
    info.static_meshes = meshes.data();
    info.num_animated_meshes = (uint32_t)anim_meshes.size();
    info.animated_meshes = anim_meshes.data();

    Renderer_ChangeScene(info);
    
    uint64_t after_uploadtogpu = SDL_GetPerformanceCounter();

    uint64_t counter_freq = SDL_GetPerformanceFrequency();
    double gpu_upload_time  = (double)(after_uploadtogpu - after_entity_load) / (double)counter_freq;
    SDL_Log("Times:\n- LoadAsset():   - Renderer_ChangeScene(): %f\n", gpu_upload_time);
}

void Scene::Update(float dt)
{
    PlayerMovement_Update(&m_ecs, dt);
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
