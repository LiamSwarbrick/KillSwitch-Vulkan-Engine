#include "game/foundations/scene.h"
#include "game/foundations/components.h"
#include "renderer/renderer.h"

// animation update
#include "core/animation.h"
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
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_StaticMesh>();
    m_ecs.RegisterComponent<C_AnimatedMesh>();

    m_prefabs.clear();

    m_physicsManager.startUp();
}

void Scene::Shutdown()
{
    m_physicsManager.shutDown();

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

EntityID Scene::InstantiatePrefab(Asset* prefab, glm::vec3 spawnPosition, glm::quat spawnRotation)
{
    if (!prefab) return MAX_ENTITIES; // or any other invalid ID

    // Root transform for the whole asset
    glm::mat4 rootMatrix = glm::translate(glm::mat4(1.0f), spawnPosition) * glm::mat4_cast(spawnRotation);

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

        if (has_ecs_data)
        { // if has ecs_data
            EntityID eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
            if (i == 0) rootEntity = eID;

            // ---------------
            // -- TRANSFORM --
            // ---------------
            
            // Finding local positions/rotations and combining with root transforms
            C_Transform t;
            glm::vec3 localPosition = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
            glm::quat localRotation = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
            glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), localPosition) * glm::mat4_cast(localRotation);
            t.matrix = rootMatrix * localTransform;

            m_ecs.AddComponent<C_Transform>(eID, { t.matrix });

            // TEMP LIGHTS:
            m_ecs.AddComponent<C_Light>(eID, {
                .type = LIGHT_COMPONENT_POINTLIGHT,
                .color = glm::vec3(0.7f, 0.7f, 1.0f),
                .intensity = 1.0f
            });
            /////////



            // 2. And put the "_ecs" value in the following
            rj::Value& components = doc["_ecs"];

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

                rbDesc.position = localPosition; // from the transform
                rbDesc.orientation = localRotation; // from the transform
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
                if(rbHandle.isValid())
                    m_ecs.AddComponent<C_RigidBody>(eID, { rbHandle });
            }

            // -- MESH
            if (node->mesh_index >= 0)
            {
                Mesh* mesh = &prefab->meshes[node->mesh_index];
                if (mesh->vertex_type == VERTEX_TYPE_SKINNED)
                {
                    uint32_t joint_count = 0;
                    if (node->skin_index >= 0)
                    {
                        joint_count = (uint32_t)prefab->skins[node->skin_index].joint_count;
                    }
                    else if (prefab->skin_count > 0)
                    {
                        joint_count = (uint32_t)prefab->skins[0].joint_count;
                    }
                    else
                    {
                        joint_count = 1;
                    }

                    C_AnimatedMesh animMesh{ mesh, prefab };
                    animMesh.joint_count = joint_count;
                    animMesh.idleAnimationName = "Idle";
                    animMesh.splitJointName = "Spine";
                    OnStartAnim(animMesh, animMesh.idleAnimationName); // Start with idle animation by default


                    if (joint_count > 0)
                    {
                        animMesh.joint_matrices = (glm::mat4*)malloc(joint_count * sizeof(glm::mat4));
                        for (uint32_t j = 0; j < joint_count; j++)
                        {
                            animMesh.joint_matrices[j] = glm::mat4(1.0f);
                        }
                    }
                    else
                    {
                        animMesh.joint_matrices = nullptr;
                    }

                    m_ecs.AddComponent<C_AnimatedMesh>(eID, std::move(animMesh));
                }
                else
                {
                    C_StaticMesh staticMesh{ &prefab->meshes[node->mesh_index], prefab };
                    m_ecs.AddComponent<C_StaticMesh>(eID, { staticMesh.mesh, staticMesh.parent_asset });
                }
            } // if (node->mesh_index >= 0)
        } // if has ecs_data
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
    m_physicsManager.update(m_ecs, dt);
    Animation_Update(&m_ecs, dt);
}

void Scene::Render()
{
    // Push light components (or light entities?) this frame
    m_ecs.GetView<C_Transform, C_Light>().ForEach([&](C_Transform& transform, C_Light& light)
    {
        glm::vec3 position = glm::vec3(transform.matrix[3]);
        glm::quat rotation = glm::quat_cast(transform.matrix);
        glm::vec3 direction = rotation * glm::vec3(0.0f, 0.0f, 1.0f);

        Renderer_PushLight(light, position, direction);
    });

    m_ecs.GetView<C_Transform, C_StaticMesh>().ForEach([&](C_Transform& transform, C_StaticMesh& mesh)
    {
        // Skip invalid / not-yet-uploaded meshes
        // (not that we support streaming yet, and if we did, textures are the actual bottleneck)
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
