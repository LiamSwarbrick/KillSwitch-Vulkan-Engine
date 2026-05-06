#ifndef PHYSICS_CORE_TYPES_H
#define PHYSICS_CORE_TYPES_H

#include "SDL3/SDL.h"

#include "core/utils/enum_bitmask.h"

#include "physics_settings.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "physics/queries/raycast.h"
#include "physics/queries/shapecast.h"


// For floats
constexpr float F_EPSILON = 1e-6f;

// ------------------
// SHAPES RELATED
// ------------------
struct AABB
{

    glm::vec3 min;
    glm::vec3 max;

    AABB() = default;

    AABB(glm::vec3 min, glm::vec3 max)
        : min(min), max(max)
    {
    }

    // paranoid functions in case the aabb wasn't centered
    // center should be glm::vec3{0.0f}
    glm::vec3 center() const { return (min + max) * 0.5f; }
    // halfWidths should be this.max
    glm::vec3 halfWidths() const { return (max - min) * 0.5f; }


    bool overlaps(const AABB& o) const
    {
        if (max.x < o.min.x || min.x > o.max.x
            || max.y < o.min.y || min.y > o.max.y
            || max.z < o.min.z || min.z > o.max.z) return false;

        return true;
    }

    // Returns true if it contains the point
    bool contains(const glm::vec3& p) const
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    // Returns true if it contains the AABB (2 points)
    bool contains(const AABB& o) const
    {
        // unoptimized, it could be but the following, im too tired to think :d
        //return o.min.x >= min.x && o.max.x <= max.x
        //    && o.min.y >= min.y && o.max.y <= max.y
        //    && o.min.z >= min.z && o.max.z <= max.z;
        return contains(o.min) && contains(o.max);
    }

    // TO generate fattened AABBs so we don't have to update the aabb every update (for dynamic rotating or animated objects)
    AABB fattened(float skin) const
    {
        return AABB(min - glm::vec3{ skin }, max + glm::vec3{ skin });
    }

    // Expand the current AABB to include the point
    void expand(glm::vec3& point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    // Merge both AABBs under one
    static AABB merge(const AABB& a, const AABB& b)
    {
        return AABB(glm::min(a.min, b.min), glm::max(a.max, b.max));
    }

    // Returns true when the ray intersects the AABB
    bool intersectsRay(const Ray& ray, RaycastHit& outHit) const
    {
        // Slab method, refer to https://education.siggraph.org/static/HyperGraph/raytrace/rtinter3.htm

        float tMin = std::numeric_limits<float>::lowest(); // Change to take into account if origin is inside the AABB
        float tMax = std::numeric_limits<float>::max();

        // Extra information for raycasthit
        int entryAxis = -1, exitAxis = -1;
        float entrySign = 1.0f, exitSign = 1.0f;

        for (int i = 0; i < 3; i++)
        {
            if (std::abs(ray.direction[i]) < F_EPSILON)
            {
                if (ray.origin[i] < min[i] || ray.origin[i] > max[i])
                    return false;
            }
            else
            {
                float invD = 1.0f / ray.direction[i]; // careful with infinite values

                float t0 = (min[i] - ray.origin[i]) * invD;
                float t1 = (max[i] - ray.origin[i]) * invD;

                float sign = 1.0f;
                if (t0 > t1)
                {
                    std::swap(t0, t1);
                    sign = -1.0f;
                }

                //tMin = std::max(tMin, t0);
                if (t0 > tMin)
                {
                    tMin = t0;
                    entryAxis = i;
                    entrySign = sign;
                }
                if (t1 < tMax)
                {
                    tMax = t1;
                    exitAxis = i;
                    exitSign = sign;
                }

                if (tMax < tMin) return false;
                if (tMax < 0.0f) return false;
            }
        }

        bool inside = tMin < 0.0f;
        float t = inside ? tMax : tMin;
        if (t < F_EPSILON || t > ray.maxDistance) return false;

        outHit.t = t;
        outHit.point = ray.origin + ray.direction * t;

        if (inside)
            outHit.normal[exitAxis] = exitSign;
        else 
            outHit.normal[entryAxis] = -entrySign;

        return true;
    }
};

enum class ShapeType : uint8_t
{
    Sphere,
    Box,
    Capsule,
    Plane,
    ConvexHull // for future (idk if we're going to use it but we CAN cause we got GJK :sunglasses:)
};

struct ShapeDesc
{
    glm::vec3 localOffset;
    glm::quat localOrientation;

    ShapeType type;

    union
    {
        struct { float radius; }                        sphere;
        struct { glm::vec3 halfWidths; }               box;
        struct { float radius; float halfHeight; }      capsule;
        struct { glm::vec3 normal; float distance; }    plane;
    };


    // For easiness at construction
    static ShapeDesc makeSphere(float radius)
    {
        ShapeDesc d;
        d.type = ShapeType::Sphere;
        d.sphere.radius = radius;

        return d;
    }

    static ShapeDesc makeBox(glm::vec3 halfWidths)
    {
        ShapeDesc d;
        d.type = ShapeType::Box;
        d.box.halfWidths = halfWidths;

        return d;
    }

    static ShapeDesc makeCapsule(float radius, float halfHeight)
    {
        ShapeDesc d;
        d.type = ShapeType::Capsule;
        d.capsule.radius = radius;
        d.capsule.halfHeight = halfHeight;

        return d;
    }

    static ShapeDesc makePlane(glm::vec3 normal, float distance)
    {
        ShapeDesc d;
        d.type = ShapeType::Plane;
        d.plane.normal = normal;
        d.plane.distance = distance;

        return d;
    }
};

// Using generic handle because im tired of doing the same handle over and over again
template <typename T>
struct PhysicsHandle
{
    uint32_t index = UINT32_MAX;


    bool isValid() const
    {
        return index != UINT32_MAX;
    }

    operator uint32_t() const
    {
        return index;
    }

    operator int() const
    {
        return static_cast<int>(index);
    }

    void operator=(uint32_t value)
    {
        index = value;
    }

    bool operator==(const T& o) const
    {
        return index == o.index;
    }

    bool operator!=(const T& o) const
    {
        return index != o.index;
    }
};

struct ShapeHandle : PhysicsHandle<ShapeHandle>
{
};

struct PlaneHandle : PhysicsHandle<PlaneHandle>
{
};

struct RigidBodyHandle : PhysicsHandle<RigidBodyHandle>
{
};

struct CharacterHandle : PhysicsHandle<CharacterHandle>
{
};

static constexpr ShapeHandle InvalidShapeHandle = { UINT32_MAX };
static constexpr PlaneHandle InvalidPlaneHandle = { UINT32_MAX };
static constexpr RigidBodyHandle InvalidRigidBodyHandle = { UINT32_MAX };
static constexpr CharacterHandle InvalidCharacterHandle = { UINT32_MAX };


// ------------------
// FORCES RELATED
// ------------------

// goofy mask functionality for different forces (i couldn't find anything other than traps for our game, wind and water are not going to be present)
enum class ForceLayer : uint32_t
{
    None = 0,
    Default = 1 << 0,
    Wind = 1 << 1,
    MagneticTrap = 1 << 2
};
DEFINE_ENUM_CLASS_BITWISE_OPERATORS(ForceLayer);


// ------------------
// RIGIDBODY RELATED
// ------------------
//enum InnerBodyLayer : uint8_t
//{
//    STATIC = 0,
//    MOVING,
//    WEAPONS
//};

// Descriptor to create RigidBodies via PhysicsManager.createBody()
struct RigidBodyDesc
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::identity<glm::quat>(); // glm::quat_identity()

    // IShape must be created first using physicsWorld.createShape (or physicsManager to delegate...)
    ShapeHandle shape = InvalidShapeHandle;

    float mass = 1.0f;
    float gravityScale = 1.0f; // Unity stuff...

    // Damping (to extend to linear and angular if i add it)
    float damping = 0.99f; // physics book

    float restitution = 0.5f;
    float friction = 0.8f;

    uint32_t forceLayers = (uint32_t) ForceLayer::Default;

    uint8_t bodyLayer = 0U;

    // RigidBody type
    bool isStatic = false;
    bool isKinematic = false;
    bool isDynamic = false; // adding this so its easier to do !isDynamic than isStatic || isKinematic || isTrigger

    bool isCharacter = false; // extra rotation locked on Y for physics (when angular momentum is added)
    bool isTrigger = false; // isTrigger will be treated as isStatic with NO collision resolution
};

struct RigidBody
{
    glm::vec3 position = glm::vec3(0.0f); // managed by ECS, imported via PhysicsManager, modified via PhysicsWorld, exported to ECS again
    glm::quat orientation = glm::identity<glm::quat>(); // managed by ECS, imported via PhysicsManager, modified via PhysicsWorld, exported to ECS again


    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 forceAccumulator = glm::vec3(0.0f);

    float mass = 1.0f;
    float invMass = 1.0f;
    float gravityScale = 1.0f;
    float damping = 0.99f;

    float restitution = 0.5f; // Might move it to global PhysicsSettings for now, but different Bodies would need this (ice, or other surfaces)
    float friction = 0.8f; // Might move it to global PhysicsSettingsfor now, but different Bodies would need this (ice, or other surfaces)

    // We could add body layer (as in player, enemy, etc, to provide better query options, but idk)
    uint32_t forceLayers = (uint32_t) ForceLayer::Default;

    // Not the same as force layer, this will enable custom collisions from game code
    // One object could be "DEBRIS" and other "MOVING" and they would not collide with each other
    uint8_t bodyLayer = 0U;

    ShapeHandle shapeHandle; // get shape via physicsWorld.getShape()

    // Out of the next block "isX" please choose only 1, not doing an enum now cause it is too much changing for everything
    bool isStatic = false;
    bool isKinematic = false;
    bool isDynamic = false; // adding this so its easier to do !isDynamic than isStatic || isKinematic || isTrigger

    bool isCharacter = false; // extra rotation locked on Y for physics (when angular momentum is added)
    bool isTrigger = false; // isTrigger will be treated as isStatic with NO collision resolution

    // DO NOT add these in the script
    // No sleep system yet
    float sleepTimer = 0.0f;
    float accumulatedEnergy = g_PhysicsSettings.initialEnergy; // smoothed using g_PhysicsSettings
    bool sleeping = false;

    void wakeUp()
    {
        if (sleeping)
        {
            sleepTimer = 0.0f;
            accumulatedEnergy = g_PhysicsSettings.initialEnergy;
            sleeping = false;
        }
    }

    AABB aabb; // maintained by broadphase

    uint32_t bodyID; // to backtrack to handle in O(1)
};

struct PhysicsCharacter
{
    enum GroundState
    {
        OnGround,
        OnSteepGround,
        InAir,
        Undefined // When getting a body that does not have a defined ground state
    };

    // Instead of pointer to body, could be RBHandle instead
    RigidBody* body = nullptr;
    
    GroundState groundState = GroundState::InAir;
    GroundState lastFrameGroundState = GroundState::InAir;
    glm::vec3 groundNormal = glm::vec3(0.0f, 1.0f, 0.0f); // use character's groundNormal to project the horizontal velocity before applying, that way we would get a much smoother movement

    glm::vec3 baseVelocity = glm::vec3(0.0f); // In case of contact with kinematic objects, we need to store this speed

    float maxWalkableAngle = glm::radians(50.0f); // Maximum angle compared to groundNormal that makes it able to snap-walk on tilted/uneven terrain
    float stepHeight = 0.4f; // Max height allowed for climbing/dropping-from steps or other obstacles

    // TO BE SET GAME-SIDE to true, it defaults to false on collision to ground
    bool jumping = true; // Extra logic to make snap-down correctly, 

    // This to do step ups and downs properly
    // Basically position and velocity before solving the contacts
    glm::vec3 lastNonWalkableNormalContact = glm::vec3(0.0f); // instead of storing if we were blocked last frame, we can store the normal to give us more information (we might not even use the extra info)
    glm::vec3 preSolvingPosition{}; // the origin of the step-up
    glm::vec3 preSolvingVelocity{}; // basically desired movement for the step-up
};

struct PhysicsCharacterInfo
{
    PhysicsCharacter::GroundState groundState = PhysicsCharacter::GroundState::Undefined;
    glm::vec3 groundNormal = glm::vec3(0.0f);
    float maxWalkableAngle = -1.0f; 
    float stepHeight = -1.0f;
};


struct BodyPair
{
    RigidBody* bodyA = nullptr;
    RigidBody* bodyB = nullptr;

    // very important ordering as we will be using sets later
    BodyPair(RigidBody* a, RigidBody* b)
    {
        if (a < b) { bodyA = a; bodyB = b; }
        else { bodyA = b; bodyB = a; }
    }

    bool operator<(const BodyPair& other) const
    {
        if (bodyA != other.bodyA)
            return bodyA < other.bodyA;
        return bodyB < other.bodyB;
    }

    bool operator==(const BodyPair& other) const
    {
        return bodyA == other.bodyA
            && bodyB == other.bodyB;
    }

    bool operator!=(const BodyPair& other) const
    {
        return !(*this == other);
    }

    bool isValid() const { return bodyA != nullptr; }
};

// Just in case BodyPair::operator< doesn't work (it has happened before)
struct BodyPairComparer
{
    bool operator()(const BodyPair& a, const BodyPair& b)
    {
        if (a.bodyA != b.bodyA)
            return a.bodyA < b.bodyA;
        return a.bodyB < b.bodyB;
    }
};


#endif // !PHYSICS_CORE_TYPES_H