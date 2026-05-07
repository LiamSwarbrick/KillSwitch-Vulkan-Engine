#include "game/foundations/scene.h"
#include "game/foundations/components.h"
#include "physics/body_layers.h"
#include "renderer/renderer.h"

// animation update
#include "core/animation.h"
#include "game/foundations/PlayerMovementSystem.h"
// RapidJSON 
#include "rapidjson/document.h"
// Imported components for automated de-serialization
#include "imported_components.h"

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
    m_ecs.RegisterComponent<C_PlayerInput>();
    m_ecs.RegisterComponent<C_CharacterController>();
    m_ecs.RegisterComponent<C_RigidBody>();
    m_ecs.RegisterComponent<C_Weapon>();

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

                rbDesc.position = localPosition; // from the transform
                rbDesc.orientation = localRotation; // from the transform
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

            if (components.HasMember("PlayerInput"))
            {
                m_ecs.AddComponent<C_PlayerInput>(eID, {});
                m_ecs.AddComponent<C_CharacterController>(eID, {
                    /*.move_speed = 0.3f*/
                });
            }

            if (components.HasMember("WeaponComponent"))
            {
                m_ecs.AddComponent<C_Weapon>(eID, { 
                    .equipped = true,
                    .attach_bone_name = "hand" 
                });
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
                    animMesh.splitJointName = "SPINE";
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
    //PlayerMovement_Update(&m_ecs, dt);

    UpdatePlayer(dt);
    
    m_physicsManager.update(m_ecs, dt);

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
        glm::vec3 position = glm::vec3(transform.matrix[3]);
        glm::quat rotation = glm::quat_cast(transform.matrix);
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

void Scene::UpdatePlayer(float dt)
{

    // BIG IMPORTANT NOTE(jaime):
    // WE NEED A fookin' class for this. if the player "crouches" we NEED to have a smaller capsule, 
    // and i would appreciate if we had all IShapes stored in advance instead of creating them on the fly
    // (PLEASE ALL CHARACTER SHAPES HAVE TO BE capsules please i beg you, otherwise physics die )

    if (m_currentPlayer == NULL_ENTITY) return;
    if (!m_ecs.Has(m_currentPlayer)) return;
    PhysicsCharacter* player = m_physicsManager.getCharacter(m_currentPlayer);
    if (!player) return;

    C_Transform& transform = m_ecs.GetComponent<C_Transform>(m_currentPlayer);
    C_PlayerInput& input = m_ecs.GetComponent<C_PlayerInput>(m_currentPlayer);
    C_CharacterController& controller = m_ecs.GetComponent<C_CharacterController>(m_currentPlayer);

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform.matrix, scale, rotation, translation, skew, perspective);

    // Flatten the camera forward vector so player movement stays horizontal.
    auto flattenDirection = [](const glm::vec3& direction)
        {
            glm::vec3 flattened = glm::vec3(direction.x, 0.0f, direction.z);
            float len = glm::length(flattened);
            if (len <= 0.0001f)
                return glm::vec3(0.0f, 0.0f, -1.0f);

            return flattened / len;
        };

    // moving forward is now relative to the camera
    glm::vec3 cameraForward = -flattenDirection(m_movementCameraForward);
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraForward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 horizontalMoveDir(0.0f);

    if (input.move_forward)
        horizontalMoveDir -= cameraForward;
    if (input.move_backward)
        horizontalMoveDir += cameraForward;
    if (input.move_left)
        horizontalMoveDir += cameraRight;
    if (input.move_right)
        horizontalMoveDir -= cameraRight;

    bool isMoving = glm::length(horizontalMoveDir) > 0.0f;
    if (isMoving)
    {
        horizontalMoveDir = glm::normalize(horizontalMoveDir);
        // update rotation 
        glm::vec3 facingDir = glm::normalize(glm::vec3(horizontalMoveDir.x, 0.0f, horizontalMoveDir.z));
        float yawDeg = glm::degrees(atan2f(facingDir.x, facingDir.z));
        rotation = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));

        transform.matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    }
    
    // If we are jumping, we need to check IF we touch the floor
    // We take away time from the jumping cooldown using dt to check next step
    if (controller.jumping)
    {
        controller.jumping_cooldown -= dt;
    }
        

    // We only need to check if the jumping cooldown is done
    if (controller.jumping_cooldown < 0.0f)
    {
        // Raycast to update if we are jumping
        // For that, we need the current Character's transform AND shape to find the feet, and then throw a very tiny ray
        //IShape* shape = m_physicsManager.getShape(m_currentPlayer);
        //ShapeType shapeType = shape->getType();
        //if (shapeType != ShapeType::Capsule)
        //{
        //    SDL_assert(false && "Character collider (IShape) needs to be of ShapeType::Capsule");
        //    return;
        //}
        //CapsuleShape* capsuleShape = static_cast<CapsuleShape*>(shape);
        //
        //Ray feetRay;
        //feetRay.origin = translation; // We will have to add the capsule's halfHeight +(-) radius to get the feet
        //feetRay.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        //feetRay.maxDistance = capsuleShape->halfHeight + capsuleShape->radius + 0.15f; // add a small distance to the halfheight + radius

        //QueryFilterExternal filter;
        //filter.bodyToIgnore = m_currentPlayer;
        //filter.hasLayerOfQuery = true;
        //filter.layerOfQuery = (uint8_t)BodyLayer::MOVING;

        //std::vector<EntityRaycastHit> hits = m_physicsManager.raycastAll(feetRay, filter);

        if (/*!hits.empty() || */player->groundState == PhysicsCharacter::GroundState::OnGround)
        {
            controller.jumping = false;
            controller.jumping_cooldown = 0.0f;
        } // If there are hits, we are in the ground
    } // If the jumping cooldown is done, we need to check if we can jump again

    // can change it to only run forwards and have other animation for runnign sideways, backwards
    bool isRunning = input.run && isMoving && !input.aim;
    if (isMoving)
    {
        // Project horizontal velocity to character's slope normal
        glm::vec3 slopeRight = glm::normalize(glm::cross(horizontalMoveDir, player->groundNormal));
        glm::vec3 slopeForward = glm::normalize(glm::cross(player->groundNormal, slopeRight));

        if (slopeForward.y > 0.0f) slopeForward.y = 0.0f; // Do this to walk down surfaces correctly.
        //slopeForward.y = 0.0f;

        // slow down when aiming
        float aim_modifier = input.aim ? 0.4f : 1.0f;
        float active_speed = isRunning ? (controller.move_speed * 5.0f) : (controller.move_speed * aim_modifier); //make it work with new physics
        
        controller.velocity = slopeForward * active_speed;
    }
    else
    {
        controller.velocity = horizontalMoveDir * controller.move_speed;
    }
    

    // Important to add the velocity to the current one, NOT WITH addVelocity cause it would linearly add to it
    glm::vec3 currentVelocity = m_physicsManager.getVelocity(m_currentPlayer);
    controller.velocity.y += currentVelocity.y;

    switch (player->groundState)
    {
    case PhysicsCharacter::GroundState::OnGround:

        if(!(!isMoving && currentVelocity.x == 0.0f && currentVelocity.z == 0.0f))
            m_physicsManager.setVelocity(m_currentPlayer, controller.velocity);

        if (input.jump)
        {
            player->jumping = true; // very important
            controller.jumping = true;
            controller.jumping_cooldown = 0.2f; // Adjust to avoid multiple-consecutive-frame / jittery jumping

            glm::vec3 gravity = m_physicsManager.getGravity();
            m_physicsManager.addVelocity(m_currentPlayer, -(gravity * 0.5f)); // Adjust jump strength as you wish
        }

        break;
    case PhysicsCharacter::GroundState::OnSteepGround:
        break;
    case PhysicsCharacter::GroundState::InAir:
        break;
    default:
        break;
    }

    //C_AnimatedMesh& animatedMesh = m_ecs.GetComponent<C_AnimatedMesh>(m_currentPlayer);

    // Play animations
    if (m_ecs.Has<C_AnimatedMesh>(m_currentPlayer))
    {
        C_AnimatedMesh& animatedMesh = m_ecs.GetComponent<C_AnimatedMesh>(m_currentPlayer);
        if (isMoving)
        {
            const char* animStateName = isRunning ? "run" : "walk";
            int moveAnimId = GetAnimationIdFromName(animatedMesh, animStateName);

            if (moveAnimId != -1 && animatedMesh.lowerBodyLayer.currentAnimation != moveAnimId)
            {
                PlayAnim(animatedMesh, animatedMesh.asset->animations[moveAnimId].name, 0.4f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
        else
        {
            int idleAnimId = GetAnimationIdFromName(animatedMesh, animatedMesh.idleAnimationName);
            if (animatedMesh.lowerBodyLayer.currentAnimation != idleAnimId)
            {
                PlayAnim(animatedMesh, animatedMesh.idleAnimationName, 0.4f);
                animatedMesh.lowerBodyLayer.isCurrentLooping = true;
            }
        }
    }

    // find equipped weapon and attach to hand
    C_AnimatedMesh& animatedMesh = m_ecs.GetComponent<C_AnimatedMesh>(m_currentPlayer);
    m_ecs.GetView<C_Weapon, C_Transform>().ForEach([&](C_Weapon& weapon, C_Transform& weaponTransform)
    {
        if (!weapon.equipped)
            return;

        int handNodeIndex = -1;

        // find node for hand
        for (uint32_t i = 0; i < animatedMesh.asset->node_count; i++)
        {
            Node* node = &animatedMesh.asset->nodes[i];

            if (node->name && strcmp(node->name, "mixamorig:RightHand") == 0)
            {
                handNodeIndex = (int)i;
                break;
            }
        }
         
        glm::mat4 handMatrix = animatedMesh.joint_matrices[handNodeIndex]; 
        weaponTransform.matrix = transform.matrix * handMatrix;
    });
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
