#ifndef PHYSICS_CORE_TYPES_H
#define PHYSICS_CORE_TYPES_H

#include "SDL3/SDL.h"

#include "core/utils/enum_bitmask.h"
#include "core/my_c_runtime.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

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
    // halfSizes should be this.max
    glm::vec3 halfSizes() const { return (max - min) * 0.5f; }


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


    // TODO: For the future when raycasting
    bool intersectsRay(glm::vec3& origin, glm::vec3& direction, float maxDistance, float& t)
    {
        SDL_assert(false && "NEED TO IMPLEMENT!!!");
        return false;
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
    ShapeType type;

    union
    {
        struct { float radius; }                        sphere;
        struct { glm::vec3 halfExtents; }               box;
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

    static ShapeDesc makeBox(glm::vec3 halfExtents)
    {
        ShapeDesc d;
        d.type = ShapeType::Box;
        d.box.halfExtents = halfExtents;

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

struct ShapeHandle
{
    uint32_t index = UINT32_MAX;

    bool isValid() const 
    { 
        return index != UINT32_MAX; 
    }

    bool operator==(const ShapeHandle& o) const 
    { 
        return index == o.index; 
    }

    bool operator!=(const ShapeHandle& o) const
    {
        return index != o.index;
    }
};

// Identical to ShapeHandle or RigidBodyHandle
struct PlaneHandle
{
    uint32_t index = UINT32_MAX;

    bool isValid() const
    {
        return index != UINT32_MAX;
    }

    bool operator==(const PlaneHandle& o) const
    {
        return index == o.index;
    }

    bool operator!=(const PlaneHandle& o) const
    {
        return index != o.index;
    }
};

static constexpr ShapeHandle InvalidShapeHandle = { UINT32_MAX };
static constexpr PlaneHandle InvalidPlaneHandle = { UINT32_MAX };




// ------------------
// FORCES RELATED
// ------------------

// goofy mask functionality for different forces (i couldn't find anything other than traps for our game, wind and water are not going to be present)
enum class ForceLayer : uint32_t
{
    None = 0,
    Default = 1 << 0,
    MagneticTrap = 1 << 1
};
DEFINE_ENUM_CLASS_BITWISE_OPERATORS(ForceLayer);


// ------------------
// RIGIDBODY RELATED
// ------------------
struct RigidBodyDesc
{
    glm::vec3 position = glm::vec3{};
    glm::quat orientation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f}; // glm::quat_identity()

    // IShape must be created first using physicsWorld.createShape (or physicsManager to delegate...)
    ShapeHandle shape = InvalidShapeHandle;

    float mass = 1.0f;
    float gravityScale = 1.0f; // Unity stuff...

    // Damping (to extend to linear and angular if i add it)
    float damping = 0.99f; // physics book

    ForceLayer forceLayers = ForceLayer::Default;

    // RigidBody type
    bool isStatic = false;
    bool isKinematic = false;
    bool isCharacter = false;
    bool isTrigger = false;
};


struct RigidBody
{
    glm::vec3 position = glm::vec3{ 0.0f };
    glm::quat orientation = glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f };

    glm::vec3 velocity = glm::vec3{ 0.0f };
    glm::vec3 forceAccumulator = glm::vec3{ 0.0f };

    float mass = 1.0f;
    float invMass = 1.0f;
    float gravityScale = 1.0f;
    float damping = 0.99f;

    uint32_t forceLayers = (uint32_t)ForceLayer::Default;

    ShapeHandle shapeHandle; // get shape via physicsWorld.getShape()

    bool isStatic = false;
    bool isKinematic = false;
    bool isCharacter = false; // rotation locked for physics
    bool isTrigger = false;

    // No sleep system yet
    bool sleeping = false;

    AABB aabb; // maintained by broadphase
};

struct RigidBodyHandle
{
    uint32_t index = UINT32_MAX;

    bool isValid() const
    {
        return index != UINT32_MAX;
    }

    bool operator==(const RigidBodyHandle& o) const
    {
        return index == o.index;
    }

    bool operator!=(const RigidBodyHandle& o) const
    {
        return index != o.index;
    }
};

static constexpr RigidBodyHandle InvalidRigidBodyHandle = { UINT32_MAX };


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

#endif // !PHYSICS_CORE_TYPES_H