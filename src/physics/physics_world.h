#ifndef PHYSICS_PHYSICS_WORLD_H
#define PHYSICS_PHYSICS_WORLD_H



// Sparse set for our rigidbodies (and shapes and planes)
#include "core/ecs/sparse_set.h"

#include "physics/core/types.h"

#include "physics/collision/broadphase/broadphase.h"
#include "physics/collision/narrowphase/narrowphase.h"
#include "physics/collision/filters/body_layer_filter.h"

#include "simulation/integrator.h"
#include "simulation/solver.h"
#include "simulation/force_registry.h"
#include "simulation/force_generators.h"

#include "queries/query_filter.h"
#include "queries/raycast.h"

#include "collision/shapes/shape.h"
#include "collision/shapes/plane.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include <vector>
#include <set>
#include <span>
#include <memory>
#include <unordered_map>


class PhysicsWorld
{
	friend class Solver; // add as friend class to access character sparseset

public:
	explicit PhysicsWorld(uint8_t numBodyLayers = 16U, uint32_t expectedBodies = 1024);
	~PhysicsWorld();

	// Non-copyable constructors
	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	// Method to reset bodies
	void clear();


	// To sync the data in & out
	struct TransformData
	{
		glm::vec3 position;
		glm::quat orientation;

		RigidBodyHandle handle;
	};

	void syncTransformsIn(std::span<const TransformData> transforms);
	void syncTransformsOut(std::span<TransformData> outTransforms);

	void update(float dt);


	// ------------------------------
	// GETTERS & SETTERS FOR BODIES AND CHARACTER
	// ------------------------------
	glm::vec3 getVelocity(RigidBodyHandle r);
	float getGravityScale(RigidBodyHandle r);
	uint32_t getForceLayers(RigidBodyHandle r);
	ShapeHandle getShapeHandle(RigidBodyHandle r);
	IShape* getShape(RigidBodyHandle r); // extra

	void setVelocity(RigidBodyHandle r, glm::vec3 velocity);
	void addVelocity(RigidBodyHandle r, glm::vec3 velocity);
	void setGravityScale(RigidBodyHandle r, float scale);
	void setForceLayers(RigidBodyHandle r, uint32_t layers);
	void addForceLayers(RigidBodyHandle r, uint32_t layers);
	void removeForceLayers(RigidBodyHandle r, uint32_t layers);

	// IMPORTANT, FOR CHECKING BODIES IN BROADPHASE
	void setNumLayers(uint8_t numLayers);
	void setLayerPair(uint8_t a, uint8_t b, bool shouldCollide);
	void enableLayerPair(uint8_t a, uint8_t b);
	void disableLayerPair(uint8_t a, uint8_t b);

	// Characters
	PhysicsCharacter::GroundState getCharacterGroundState(RigidBodyHandle r);
	// No point in setting the ground state manually really, its just an indicator, it does not have a purpose inside the solver
	
	float getCharacterMaxWalkableAngle(RigidBodyHandle r);
	void setCharacterMaxWalkableAngle(RigidBodyHandle r, float maxWalkableAngle);
	float getCharacterStepHeight(RigidBodyHandle r);
	void setCharacterStepHeight(RigidBodyHandle r, float stepHeight);

	// ------------------------------
	// BODY MANAGEMENT
	// ------------------------------
	// This specific call will be to have the handleIndexToReserve be the same as the EntityID
	RigidBodyHandle addBody(const RigidBodyDesc& desc);
	void removeBody(RigidBodyHandle handle);
	RigidBody* getBody(RigidBodyHandle handle);
	const RigidBody* getBody(RigidBodyHandle handle) const;
	void setBodyShape(RigidBodyHandle handle, ShapeHandle shape);

	PhysicsCharacter* getCharacter(RigidBodyHandle r);
private:
	inline CharacterHandle getCharacterHandle(RigidBodyHandle r);
	void addCharacter(RigidBody* body);
	void removeCharacter(RigidBodyHandle r);

public:
	void setCharacterInfo(RigidBodyHandle r, const PhysicsCharacterInfo& info);

	// ------------------------------
	// SHAPE MANAGEMENT
	// ------------------------------
	ShapeHandle createShape(const ShapeDesc& desc);
	// Destroy shape should only be called for shape queries, it is automatically called on body deletion
	void destroyShape(ShapeHandle handle); 
	IShape* getShape(ShapeHandle handle);
	const IShape* getShape(ShapeHandle handle) const;

	// ------------------------------
	// PLANE MANAGEMENT (because we have a floor. it could be terrain, but would change things)
	// ------------------------------
	PlaneHandle addPlane(const ShapeDesc& desc);
	void removePlane(PlaneHandle handle);

	// ------------------------------
	// FORCE REGISTRY
	// ------------------------------

	// Global generators
	void addGenerator(IForceGenerator* gen);
	void removeGenerator(IForceGenerator* gen);

	// Paired generators
	void addForce(RigidBodyHandle handle, IForceGenerator* gen);
	void removeForce(RigidBodyHandle handle, IForceGenerator* gen);


	// ------------------------------
	// QUERIES (TODO)
	// ------------------------------
	RaycastHit raycast(const Ray& ray, const QueryFilter& filter = {}) const;

	std::vector<RaycastHit> raycastAll(const Ray& ray, const QueryFilter& filter = {}) const;

	// Shape - intersecting
	std::vector<RigidBodyHandle> shapeIntersects(
		ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation,
		const QueryFilter& filter = {}) const;

	

	// ------------------------------
	// EVENTS
	// ------------------------------
	// These are going to be single subscriber functions.
	// The one who should subscribe to these is the PhysicsManager, nobody else.
	// Then the PhysicsManager will be the one to propagate the event (either him, or subscribe these to a global bus)
	std::function<void(RigidBodyHandle a, RigidBodyHandle b, const Contact& contact)> onCollisionEnter;
	std::function<void(RigidBodyHandle a, RigidBodyHandle b, const Contact& contact)> onCollisionStay;
	std::function<void(RigidBodyHandle a, RigidBodyHandle b)> onCollisionExit;

	std::function<void(RigidBodyHandle a, RigidBodyHandle b, const Contact& contact)> onTriggerEnter;
	std::function<void(RigidBodyHandle a, RigidBodyHandle b, const Contact& contact)> onTriggerStay;
	std::function<void(RigidBodyHandle a, RigidBodyHandle b)> onTriggerExit;


	// ------------------------------
	// SETTERS & GETTERS
	// ------------------------------
	void setGravity(glm::vec3 gravity);
	glm::vec3 getGravity() const;

	void setFixedStep(float step);
	float getFixedStep() const;

	void setMaxSteps(int max);
	int getMaxSteps() const;

private:
	// ------------------------------
	// MAIN LOGIC
	// ------------------------------
	void step(float dt);
	void applyForces(float dt);
	void integrate(float dt);
	void updateBroadPhase();
	void detectCollisions(); // broadPhase.queryPairs() + narrowPhase.testPair(<pair>)
	void testPlanes(); // narrowPhase.testPlane(<bodies, planes>)
	void solve(float dt); // solve interpenetration


	// TODO after implementing events
	void dispatchEvents();

private:
	// Helper for AABB
	void calculateAABB(RigidBody* body);

	// Helper to translate QueryFilter to QueryFilterInternal
	QueryFilterInternal getQueryFilterInternalFromQueryFilter(const QueryFilter& queryFilter) const;

	//inline RigidBody& getBody();

private:
	// --- SYSTEMS ---
	BroadPhase broadPhase;
	BodyLayerFilter bodyLayerFilter;
	NarrowPhase narrowPhase;
	Integrator integrator;
	Solver solver{ *this };
	ForceRegistry forceRegistry;

	// --- POOLS (to refactor ECS to remove namespace and move definitions to .cpp)
	// Fairly similar to the ECS registry, we will just have a couple sparse sets for shapes (though planes is a little bit overkill)
	SparseSet<RigidBody> bodies;
	std::vector<uint32_t> freeBodyIndices;

	SparseSet<PhysicsCharacter> characters;
	std::vector<uint32_t> freeCharacterIndices;

	std::unordered_map<uint32_t, CharacterHandle> bodyToCharacter;

	// i will use a sparseset of pointers... for polymorphism, even thought we could have a better data structure
	// a pool would suffice, because we do not worry about cache friendliness when it comes to shapes
	// we're not going to iterate over shapes, they are going to be accesed from rigidbodies
	struct ShapeRef
	{
		IShape* shape = nullptr;
		uint32_t refCount = 0;
	};

	ShapeRef* getShapeRef(ShapeHandle handle);
	void retainShape(ShapeHandle handle);
	void releaseShape(ShapeHandle handle);
	void clearShapeRefs(); // Have to free the memory using delete manually

	SparseSet<ShapeRef> shapes;
	std::vector<uint32_t> freeShapeIndices;

	SparseSet<PlaneShape> planes;
	std::vector<uint32_t> freePlaneIndices;

	// --- EACH FRAME ---
	// we keep it here with .reserve() so we don't allocate too much in the step.
	std::vector<Contact> contacts;

	// FOR EVENTS: previous frame. to detect OnEnter, OnStay, OnExit
	std::set<BodyPair> previousCollisionPairs;
	std::set<BodyPair> previousTriggerPairs;

	// --- SETTINGS ---
	GravityGenerator gravityGen; // default gravity
	float fixedStep = 1.0f / 120.0f;
	float stepAccumulator = 0.0f;
	int maxSteps = 4;

	glm::vec3 UP_VECTOR = glm::vec3(0.0f, 1.0f, 0.0f);
};

#endif // !PHYSICS_PHYSICS_WORLD_H