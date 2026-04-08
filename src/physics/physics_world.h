#ifndef PHYSICS_PHYSICS_WORLD_H
#define PHYSICS_PHYSICS_WORLD_H



// Sparse set for our rigidbodies (and shapes and planes)
#include "core/ecs/sparse_set.h"

#include "physics/core/types.h"

#include "broadphase/broadphase.h"
#include "narrowphase/narrowphase.h"
#include "simulation/integrator.h"
#include "simulation/solver.h"
#include "simulation/force_registry.h"
#include "simulation/force_generators.h"

#include "collision/shapes/shape.h"
#include "collision/shapes/plane.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include <set>
#include <span>
#include <memory>

class PhysicsWorld
{
public:
	explicit PhysicsWorld(uint32_t expectedBodies = 1024);
	~PhysicsWorld();

	// Non-copyable constructors
	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;


	// To sync the data in & out
	struct TransformData
	{
		glm::vec3 position;
		glm::quat orientation;

		RigidBodyHandle handle;
	};

	void syncTransformsIn(std::span<const TransformData> transforms);
	void syncTransformsOut(std::span<TransformData> transforms);

	void update(float dt);

	// ------------------------------
	// BODY MANAGEMENT
	// ------------------------------
	// This specific call will be to have the handleIndexToReserve be the same as the EntityID
	RigidBodyHandle addBody(const RigidBodyDesc& desc);
	void removeBody(RigidBodyHandle handle);
	RigidBody* getBody(RigidBodyHandle handle);
	const RigidBody* getBody(RigidBodyHandle handle) const;
	void setBodyShape(RigidBodyHandle handle, ShapeHandle shape);


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

	// ------------------------------
	// EVENTS (TODO)
	// ------------------------------

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
	// void dispatchEvents();

private:
	// Helper for AABB
	void calculateAABB(RigidBody* body);

private:
	// --- SYSTEMS ---
	BroadPhase broadPhase;
	NarrowPhase narrowPhase;
	Integrator integrator;
	Solver solver;
	ForceRegistry forceRegistry;

	// --- POOLS (to refactor ECS to remove namespace and move definitions to .cpp)
	// Fairly similar to the ECS registry, we will just have a couple sparse sets for shapes (though planes is a little bit overkill)
	SparseSet<RigidBody> bodies;
	std::vector<uint32_t> freeBodyIndices;

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
	
	SparseSet<ShapeRef> shapes;
	std::vector<uint32_t> freeShapeIndices;

	SparseSet<PlaneShape> planes;
	std::vector<uint32_t> freePlaneIndices;

	// --- EACH FRAME ---
	// we keep it here with .reserve() so we don't allocate too much in the step.
	std::vector<Contact> contacts;

	// previous frame. to detect OnEnter, OnStay, OnExit
	std::set<BodyPair> previousCollisionPairs;
	std::set<BodyPair> previousTriggerPairs;

	// --- SETTINGS ---
	GravityGenerator gravityGen; // default gravity
	float fixedStep = 1.0f / 120.0f;
	float stepAccumulator = 0.0f;
	int maxSteps = 4;
};

#endif // !PHYSICS_PHYSICS_WORLD_H