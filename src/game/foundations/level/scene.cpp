#include "game/foundations/scene.h"
#include "game/foundations/components.h"
#include "physics/body_layers.h"
#include "renderer/renderer.h"

// animation update
#include "core/animation.h"
// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "imported_components.h"

// Include all needed systems
#include "game/systems/EnemyAISystem.h"
#include "game/systems/PlayerInputSystem.h"
#include "game/systems/MovementSystem.h"
#include "game/systems/PhysicsSystem.h"
#include "game/systems/AnimationSystem.h"
#include "game/systems/CombatSystem.h"
#include "game/systems/HealthSystem.h"
#include "game/systems/DespawnSystem.h"

// types from physics
#include "physics/core/types.h"

#include "SDL3/SDL.h"

// To decompose the matrix
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"


namespace rj = rapidjson;

void Scene::StartUp()
{
    m_ecs.RegisterComponent<C_Transform>();
    m_ecs.RegisterComponent<C_Light>();
    m_ecs.RegisterComponent<C_StaticMesh>();
    m_ecs.RegisterComponent<C_AnimatedMesh>();
    //m_ecs.RegisterComponent<C_PlayerController>();
    //m_ecs.RegisterComponent<C_PlayerInput>();
    m_ecs.RegisterComponent<C_EnemyAIInfo>();
    m_ecs.RegisterComponent<C_RigidBody>();
    m_ecs.RegisterComponent<C_MovementInput>();
    m_ecs.RegisterComponent<C_MovementStats>();
    m_ecs.RegisterComponent<C_MovementInfo>();
    m_ecs.RegisterComponent<C_CombatInput>();
    m_ecs.RegisterComponent<C_WeaponSocket>();

    m_prefabs.clear();

    m_physicsManager.startUp();
    SetBodyCollisionLayers();

    auto printCollision = [](CollisionEnterAndStayArgs args)
    {
            /*std::cout << "Collision: [" << args.a << "-" << args.b
                << "], Point: [" << args.contact.point.x << ", " << args.contact.point.y << ", " << args.contact.point.z
                << "]  Normal: [" << args.contact.normal.x << ", " << args.contact.normal.y << ", " << args.contact.normal.z << "]"
                << std::endl;*/
    };

    // Both ways of subscribing
    m_onCollisionEnterSubscription = m_physicsManager.onCollisionEnter += printCollision; 
    m_onCollisionStaySubscription = m_physicsManager.onCollisionStay.Subscribe(printCollision);

    // Important bit
    InitSystems();
}

void Scene::Shutdown()
{
    // Unsubscribe before shutting down
    m_onCollisionEnterSubscription.Unsubscribe();
    m_onCollisionStaySubscription.Unsubscribe();
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

        // Finding local positions/rotations and combining with root transforms
        C_Transform t;
        glm::vec3 localPosition = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        glm::quat localRotation = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), localPosition) * glm::mat4_cast(localRotation);
        t.matrix = rootMatrix * localTransform;
        
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(t.matrix, scale, rotation, translation, skew, perspective);

        if (node->light_index >= 0)
        {
            // LIGHT SOURCE ENTITY (just a transform and light properties)
            EntityID eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
            if (rootEntity == MAX_ENTITIES) rootEntity = eID;

            // ---------------
            // -- TRANSFORM --
            // ---------------
            m_ecs.AddComponent<C_Transform>(eID, { t.matrix });

            // ------------------
            // -- LIGHT SOURCE --
            // ------------------
            SDL_assert(node->light_index < prefab->light_count);
            Light light_data = prefab->lights[node->light_index];
            LightComponentType light_type;
            switch (light_data.type)
            {
                case 2: light_type = LIGHT_COMPONENT_POINTLIGHT; break;
                case 3: light_type = LIGHT_COMPONENT_SPOTLIGHT; break;
                default: SDL_assert(0 && "Unimplemented light type detected (directional/area lights not implemented yet).");
            }
            
            SDL_assert(light_data.range >= 0.0f && "Make sure in Blender to set the custom distance of the light, otherwise no culling can occurr");
            m_ecs.AddComponent<C_Light>(eID, {
                .type = light_type,
                .color = glm::vec3(light_data.color[0], light_data.color[1], light_data.color[2]),
                .intensity = light_data.intensity,
                .radius = light_data.range,
                .spot_inner_cone_angle = light_data.spot_inner_cone_angle,
                .spot_outer_cone_angle = light_data.spot_outer_cone_angle
            });
        }
        else if (has_ecs_data)
        { // if has ecs_data
            EntityID eID = m_ecs.CreateEntity((node->name) ? (node->name) : "");
            if (rootEntity == MAX_ENTITIES) rootEntity = eID;

            // ---------------
            // -- TRANSFORM --
            // ---------------
            m_ecs.AddComponent<C_Transform>(eID, { t.matrix });


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

                rbDesc.position = translation; // from the transform
                rbDesc.orientation = rotation; // from the transform
                rbDesc.mass = importedRigidbody.mass;
                rbDesc.gravityScale = importedRigidbody.gravity_scale;
                rbDesc.damping = importedRigidbody.damping;
                rbDesc.restitution = 0.0f;
                rbDesc.friction = 0.2f;
                rbDesc.forceLayers = importedRigidbody.force_layers;

                // Important bit
                bool isSomething = false;

                SDL_assert(!(isSomething && importedRigidbody.is_static) && "RigidBody cannot be 2 'isSomething' at the same time");
                isSomething = isSomething || importedRigidbody.is_static;
                rbDesc.isStatic = importedRigidbody.is_static;

                SDL_assert(!(isSomething && importedRigidbody.is_kinematic) && "RigidBody cannot be 2 'isSomething' at the same time");
                isSomething = isSomething || importedRigidbody.is_kinematic;
                rbDesc.isKinematic = importedRigidbody.is_kinematic;

                SDL_assert(!(isSomething && importedRigidbody.is_character) && "RigidBody cannot be 2 'isSomething' at the same time");
                isSomething = isSomething || importedRigidbody.is_character;
                rbDesc.isCharacter = importedRigidbody.is_character;

                SDL_assert(!(isSomething && importedRigidbody.is_trigger) && "RigidBody cannot be 2 'isSomething' at the same time");
                isSomething = isSomething || importedRigidbody.is_trigger;
                rbDesc.isTrigger = importedRigidbody.is_trigger;

                if(!isSomething)
                    rbDesc.isDynamic = !isSomething; // if we are nothing (from the importedRigidbody) then we are dynamic;

                // TEMPORARY automated body layers if they are not in the script
                if (importedRigidbody.is_static || importedRigidbody.is_trigger || importedRigidbody.is_kinematic)
                    rbDesc.bodyLayer = (uint8_t) BodyLayer::STATIC;
                else if(importedRigidbody.is_character)
                    rbDesc.bodyLayer = (uint8_t)BodyLayer::CHARACTER;
                else // only dynamic bodies, kinematic bodies will be treated in the STATIC layer
                    rbDesc.bodyLayer = (uint8_t) BodyLayer::MOVING;

                rbDesc.shape = shapeHandle;

                RigidBodyHandle rbHandle = m_physicsManager.createBody(eID, rbDesc);

                // 3.3 If we had more data to acces (in the node), feel free to add data to your component, 
                // but that won't probably be the case for player defined components

                // 3.4 Finally add the component to the ECS!!!
                if(rbHandle.isValid())
                    m_ecs.AddComponent<C_RigidBody>(eID, { rbHandle });
            } // if rigidbodycomponent

            if ((components.HasMember("PlayerInput") || components.HasMember("ZombieInput")))
            {
                if (components.HasMember("PlayerInput"))
                {
                    m_ecs.AddComponent<C_PlayerInput>(eID);
                    m_ecs.AddComponent<C_PlayerInfo>(eID);

                    m_ecs.AddComponent<C_WeaponSocket>(eID, {
                        .equipped = false,
                        .attach_bone_name = "hand"
                    });

                    m_ecs.AddComponent<C_MovementStats>(eID, C_MovementStats::DefaultPlayerStats());
                    m_ecs.AddComponent<C_Faction>(eID, { FactionType::Player });
                    m_ecs.AddComponent<C_CombatMeleeStats>(eID, C_CombatMeleeStats::PlayerDefaultCombatStats());
                    m_ecs.AddComponent<C_Health>(eID, C_Health::PlayerDefaultHealth());

                }
                if (components.HasMember("ZombieInput"))
                {
                    // No need for input as it is processed in the EnemyAISystem
                    m_ecs.AddComponent<C_EnemyAIStats>(eID);
                    m_ecs.AddComponent<C_EnemyAIInfo>(eID);
                    m_ecs.AddComponent<C_MovementStats>(eID, C_MovementStats::DefaultZombieStats());
                    m_ecs.AddComponent<C_Faction>(eID, { FactionType::Zombie });
                    m_ecs.AddComponent<C_CombatMeleeStats>(eID, C_CombatMeleeStats::ZombieDefaultCombatStats());
                    m_ecs.AddComponent<C_Health>(eID, C_Health::ZombieDefaultHealth());
                }

                m_ecs.AddComponent<C_MovementInput>(eID);
                m_ecs.AddComponent<C_MovementInfo>(eID);
                m_ecs.AddComponent<C_CombatInput>(eID);
                m_ecs.AddComponent<C_CombatInfo>(eID);
            }

            if (components.HasMember("WeaponComponent"))
            {
                // Add the default pistol for now (we don't have other weapons
                m_ecs.AddComponent<C_WeaponRanged>(eID, C_WeaponRanged::DefaultPistol());

                m_ecs.AddComponent<C_Light>(eID, FLASHLIGHT_C_LIGHT);
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
                    animMesh.idleAnimationName = "idle";
                    animMesh.splitJointName = "mixamorig:Spine";
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

    for (auto& system : m_systems)
        system->Update(dt);

    Ray ray = { glm::vec3(0.0f, 0.5f, 2.0f), glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), 10.0f};
    QueryFilterExternal filter;
    filter.hasLayerOfQuery = true;
    filter.layerOfQuery = (uint8_t) BodyLayer::WEAPON;
    auto hits = m_physicsManager.raycastAll(ray, filter);
    /*for (EntityRaycastHit hit : hits)
    {
        std::cout << "[Raycast Hit] Entity: [" << hit.entity << " | " << m_ecs.GetEntityTag(hit.entity) << "] at t : " << hit.t
            << ", point: [" << hit.point.x << "," << hit.point.y << "," << hit.point.z << "]"
            << ", normal: [" << hit.normal.x << "," << hit.normal.y << "," << hit.normal.z << "]"
            << std::endl;
    }*/
    Animation_Update(&m_ecs, dt);
}

void Scene::Render()
{
    // Push light components (or light entities?) this frame
    m_ecs.GetView<C_Transform, C_Light>().ForEach([&](C_Transform& transform, C_Light& light)
    {
        glm::quat local_rot = glm::quatLookAt(light.local_forward_dir, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat rotation = glm::quat_cast(transform.matrix) * local_rot;
        glm::vec3 position = glm::vec3(transform.matrix[3]) + (rotation * light.local_position);
        
        glm::vec3 direction = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        b32 is_shadowed = light.type == LIGHT_COMPONENT_SPOTLIGHT;
        Renderer_PushLight(light, position, direction, is_shadowed);
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

    m_ecs.GetView<C_Transform, C_AnimatedMesh, C_EnemyAIInfo>().ForEach([&](C_Transform& transform, C_AnimatedMesh& mesh, C_EnemyAIInfo& info)
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

    // SDL_Log("%u drawing anims %d\n", num_anims, SDL_GetTicks());
}

void Scene::InitSystemContext()
{
    m_systemCtx.ecs = &m_ecs;
    m_systemCtx.physicsManager = &m_physicsManager;
}

void Scene::InitSystems()
{
    InitSystemContext();
    // PLEASE MAKE SURE THEY ARE IN ORDER OF COMPONENT DEPENDENCIES :D

    // Process inputs first
    RegisterSystem<PlayerInputSystem>();
    RegisterSystem<EnemyAISystem>();

    // Then process the movement based on those inputs
    RegisterSystem<MovementSystem>();

    // Process combat right after
    RegisterSystem<CombatSystem>();

    // Process combat before physics update (so we attack what we see)
    // Maybe do a Player/ZombieCombatSystem, but CombatSystem can process both anyway (like animation has UpdatePlayer(dt);)
    //RegisterSystem<CombatSystem>();

    // Physics After movement system
    RegisterSystem<PhysicsSystem>();

    // Animation System always in the end
    RegisterSystem<AnimationSystem>();
    
    RegisterSystem<HealthSystem>();
    RegisterSystem<DespawnSystem>();
}


void Scene::SetBodyCollisionLayers()
{
    m_physicsManager.setNumLayers(NUM_BODY_LAYERS);
    //m_physicsManager.setLayerPair((uint8_t) BodyLayer::STATIC, (uint8_t) BodyLayer::STATIC, false);
    // BodyLayers
    // MOVING VS STATIC && MOVING
    // CHARACTER VS CHARACTER && MOVING && STATIC
    m_physicsManager.disableLayerPair((uint8_t) BodyLayer::STATIC, (uint8_t) BodyLayer::STATIC);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::STATIC, (uint8_t)BodyLayer::MOVING);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::MOVING, (uint8_t)BodyLayer::MOVING);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::CHARACTER, (uint8_t)BodyLayer::CHARACTER);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::CHARACTER, (uint8_t)BodyLayer::MOVING);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::CHARACTER, (uint8_t)BodyLayer::STATIC);

    // Query filter Layers
    // WEAPON VS MOVING && CHARACTER
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::WEAPON, (uint8_t)BodyLayer::MOVING);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::WEAPON, (uint8_t)BodyLayer::MOVING);
    // AFFECT_ONLY_CHARACTER VS CHARACTER (for zombie attacks)?
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::AFFECT_ONLY_CHARACTER, (uint8_t)BodyLayer::CHARACTER);
    // AFFECT_ONLY_STATIC VS STATIC
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::AFFECT_ONLY_STATIC, (uint8_t)BodyLayer::STATIC);

    // AFFECT_NOT_CHARACTER VS STATIC VS MOVING (for camera shapecasting
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::AFFECT_NOT_CHARACTER, (uint8_t)BodyLayer::STATIC);
    m_physicsManager.enableLayerPair((uint8_t)BodyLayer::AFFECT_NOT_CHARACTER, (uint8_t)BodyLayer::MOVING);

}
