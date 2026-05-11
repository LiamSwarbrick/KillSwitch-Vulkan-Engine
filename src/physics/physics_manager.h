#ifndef PHYSICS_PHYSICS_MANAGER_H
#define PHYSICS_PHYSICS_MANAGER_H

#include "core/ecs.h"
#include "core/event.h"

#include "physics/core/types.h"
#include "physics_world.h"

#include "physics/simulation/force_generators.h"
#include "physics/collision/narrowphase/contact.h"

#include "body_layers.h"

#include "glm/glm.hpp"

#include <unordered_map>

struct EntityRaycastHit
{
	glm::vec3 point;
	glm::vec3 normal;

	float t = -1.0f;
	EntityID entity = NULL_ENTITY;
	RigidBody* body = nullptr;

	bool isValid() const { return entity != NULL_ENTITY; }
};

struct EntityShapecastHit
{
	glm::vec3 point; // o .  world-space
	glm::vec3 pointA; // worldspace at t
	glm::vec3 pointB; // worldspace at t

	glm::vec3 normal; // surface normal (so B to A normal)

	float t = -1.0f;

	EntityID entity = NULL_ENTITY;
	RigidBody* body = nullptr;

	bool isValid() const { return t >= 0.0f; }

	static EntityShapecastHit none() { return {}; }
};

struct EntityShapeIntersectsHit
{
	EntityID entity = NULL_ENTITY;
	RigidBodyHandle body = InvalidRigidBodyHandle;
};

struct QueryFilterExternal
{
	EntityID bodyToIgnore = NULL_ENTITY; // The entity to ignore, if we are raycasting from the player, put the player entity here

	bool hasLayerOfQuery = false; // True if there is a body layer for querying collisions
	uint8_t layerOfQuery = 0; // If we are casting on a specific layer, that layer will collide only with the ones that has activated collisions
};

struct CollisionEnterAndStayArgs
{
	EntityID a;
	EntityID b;
	const Contact& contact;
};

struct CollisionExitArgs
{
	EntityID a;
	EntityID b;
};

class PhysicsManager
{
public:
	// ------------------------------
	// Module Initialization (if singleton) (should be singleton and have all physicsWorlds)
	// ------------------------------
	PhysicsManager() {}
	~PhysicsManager() {}

	void startUp();
	void shutDown();

	// ------------------------------
	// SCENE/LEVEL MANAGEMENT
	// ------------------------------
	void clear();
	// FOR MULTIPLE SCENES:
	// Initially i was only going to have a single physics world, but we might have different Scenes / Levels, so we might need multiple physic worlds
	// The idea would be to changeScene to the one we want, load all entities there so it keeps state, but we might not need that
	// void changeScene(int id); // changes pointer to wrapper (would need to add background simulation if needed)
	// void clearScene(int id); // clears the state but keeps ID
	// void removeScene(int id); // clears the state and removes ID (idk)


	// ------------------------------
	// BODY MANAGEMENT
	// ------------------------------
	RigidBodyHandle createBody(EntityID entity, const RigidBodyDesc& desc);
	void destroyBody(EntityID entity);
	void destroyBody(RigidBodyHandle handle);
	// do not confuse with setShape(ShapeHandle handle, ShapeDesc);
	void setBodyShape(EntityID e, ShapeHandle shapeHandle);
	void setBodyShape(RigidBodyHandle handle, ShapeHandle shapeHandle);

	PhysicsCharacter* getCharacter(EntityID entity);
	PhysicsCharacter* getCharacter(RigidBodyHandle handle);
	void setCharacterInfo(EntityID entity, const PhysicsCharacterInfo& info);
	void setCharacterInfo(RigidBodyHandle handle, const PhysicsCharacterInfo& info);

	// ------------------------------
	// SHAPE MANAGEMENT
	// ------------------------------
	ShapeHandle createShape(const ShapeDesc& desc);
	// Destroy shape should only be called for shape queries, it is automatically called on body deletion
	void destroyShape(ShapeHandle handle);

	// ------------------------------
	// HELPERS "JUST-IN-CASE" 
	// ------------------------------
	// mainly for IMGUI, but we can explore option C of exposing certain data to C_RigidBody in ECS
	const RigidBody* getBody(EntityID entity);
	const RigidBody* getBody(RigidBodyHandle handle);

	PhysicsWorld& getWorld() { return world; }


	// ------------------------------
	// SYNC & UPDATE
	// ------------------------------
	// PhysicsManager::update translates and syncs data to PhysicsWorld
	void update(ECS& ecs, float dt);


	// ------------------------------
	// GETTERS & SETTERS
	// ------------------------------
	glm::vec3 getWorldUp();

	glm::vec3 getVelocity(EntityID e);
	glm::vec3 getVelocity(RigidBodyHandle handle);
	float getGravityScale(EntityID e);
	float getGravityScale(RigidBodyHandle handle);
	uint32_t getForceLayers(EntityID e);
	uint32_t getForceLayers(RigidBodyHandle handle);
	ShapeHandle getShapeHandle(EntityID e);
	ShapeHandle getShapeHandle(RigidBodyHandle handle);
	Shape* getShape(EntityID e); // extra
	Shape* getShape(RigidBodyHandle handle); // extra

	// Please do not use teleportBody to MOVE, use it to truly TELEPORT TO A PLACE
	void teleportBody(EntityID e, const glm::vec3& worldPosition);
	void teleportBody(RigidBodyHandle handle, const glm::vec3& worldPosition);

	void setVelocity(EntityID e, const glm::vec3& velocity);
	void setVelocity(RigidBodyHandle handle, const glm::vec3& velocity);

	void addVelocity(EntityID e, const glm::vec3& velocity);
	void addVelocity(RigidBodyHandle handle, const glm::vec3& velocity);

	void setGravityScale(EntityID e, float scale);
	void setGravityScale(RigidBodyHandle handle, float scale);

	void setForceLayers(EntityID e, uint32_t layers);
	void setForceLayers(RigidBodyHandle handle, uint32_t layers);

	void addForceLayers(EntityID e, uint32_t layers);
	void addForceLayers(RigidBodyHandle handle, uint32_t layers);

	void removeForceLayers(EntityID e, uint32_t layers);
	void removeForceLayers(RigidBodyHandle handle, uint32_t layers);

	// Same methods but with handle

	glm::vec3 getGravity() const;
	
	// Important, to set the body layer's pairs for the collision matrix
	void setNumLayers(uint8_t numLayers);
	void setLayerPair(uint8_t a, uint8_t b, bool shouldCollide);
	void enableLayerPair(uint8_t a, uint8_t b);
	void disableLayerPair(uint8_t a, uint8_t b);

	// Characters
	PhysicsCharacter::GroundState getCharacterGroundState(EntityID e);
	PhysicsCharacter::GroundState getCharacterGroundState(RigidBodyHandle handle);

	float getCharacterMaxWalkableAngle(EntityID e);
	float getCharacterMaxWalkableAngle(RigidBodyHandle handle);

	void setCharacterMaxWalkableAngle(EntityID e, float maxWalkableAngle);
	void setCharacterMaxWalkableAngle(RigidBodyHandle handlee, float maxWalkableAngle);

	float getCharacterStepHeight(EntityID e);
	float getCharacterStepHeight(RigidBodyHandle handle);

	void setCharacterStepHeight(EntityID e, float stepHeight);
	void setCharacterStepHeight(RigidBodyHandle handle, float stepHeight);


	// ------------------------------
	// FORCES (REGISTRY)
	// ------------------------------
	void addGenerator(IForceGenerator* gen);
	void removeGenerator(IForceGenerator* gen);

	void addForce(EntityID entity, IForceGenerator* gen);
	void addForce(RigidBodyHandle handle, IForceGenerator* gen);

	void removeForce(EntityID entity, IForceGenerator* gen);
	void removeForce(RigidBodyHandle handle, IForceGenerator* gen);


	// ------------------------------
	// QUERIES (planning for the future)
	// ------------------------------
	// All of these methods basically translate from RigidBody*/RigidBodyHandle to EntityID
	
	EntityRaycastHit raycast(const Ray& ray, const QueryFilterExternal& filter = {}) const;
	EntityRaycastHit raycast(const Ray& ray, const QueryFilter& filter = {}) const;

	std::vector<EntityRaycastHit> raycastAll(const Ray& ray, const QueryFilterExternal& filter = {}) const;
	std::vector<EntityRaycastHit> raycastAll(const Ray& ray, const QueryFilter& filter = {}) const;

	EntityShapecastHit shapecast(
		const Ray& ray, ShapeHandle shape, const glm::quat& orientation,
		const QueryFilterExternal& filter = {}) const;

	EntityShapecastHit shapecast(
		const Ray& ray, ShapeHandle shape, const glm::quat& orientation,
		const QueryFilter& filter = {}) const;

	// Shape-casting too (might change the input to be shapeIntersects or something like that, but for now this, will see when i implement it)
	std::vector<EntityShapeIntersectsHit> shapeIntersects(
		ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation, 
		const QueryFilterExternal& filter = {}) const;

	std::vector<EntityShapeIntersectsHit> shapeIntersects(
		ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation,
		const QueryFilter& filter = {}) const;

	// ------------------------------
	// EVENTS
	// ------------------------------
	// Contrary to PhysicsWorld, where we use a single function (1 subscriber only), here we use proper events where other systems can subscribe to. 
	// If we had a global bus, the physics manager would be the one subscribe the world events there.
	Event<CollisionEnterAndStayArgs> onCollisionEnter;
	Event<CollisionEnterAndStayArgs> onCollisionStay;
	Event<CollisionExitArgs> onCollisionExit;

	Event<CollisionEnterAndStayArgs> onTriggerEnter;
	Event<CollisionEnterAndStayArgs> onTriggerStay;
	Event<CollisionExitArgs> onTriggerExit;


private:
	inline RigidBodyHandle getHandle(EntityID entity) const;
	inline EntityID getEntityID(RigidBodyHandle handle) const;

private:
	// OK FOR NOW I WILL ONLY HAVE ONE WORLD, cause if i don't, i need all the data above to reference the current world
	// And have an array of all the following data...
	// Uncomment the following for multiple scenes, and adapt the .cpp accordingly
	// 
	// if ensuring scenes will be from range 0 to 
	//std::unordered_map<int, size_t> scenesToWrapper;

	//struct PhysicsWorldWrapper
	//{
	//	PhysicsWorld world;
	//	std::unordered_map<EntityID, RigidBodyHandle> entityToHandle;
	//	std::unordered_map<RigidBodyHandle, EntityID> handleToEntity;
	//};
	//PhysicsWorldWrapper* currentWorldWrapper;
	//std::vector<PhysicsWorldWrapper> worldWrappers;

	PhysicsWorld world;
	std::unordered_map<EntityID, RigidBodyHandle> entityToHandle;
	std::unordered_map<uint32_t, EntityID> handleToEntity;

	// Yeah no, i will have to refactor physicsWorld to retrieve exclusively RBHandles intead of RBody*
	//EntityID bodyToEntity(RigidBody* body) const;

	// Need this to translate events fired from world to RigidBodyHandle to EntityID
	void bindWorldEvents();

	// Extra little helper
	inline EntityRaycastHit rayHitToEntityRayHit(const RaycastHit& rayHit) const;
	inline EntityShapecastHit shapeHitToEntityShapeHit(const ShapecastHit& shapeHit) const;
	inline QueryFilter getQueryFilterFromQueryFilterExternal(const QueryFilterExternal& queryFilterExternal) const;
};

#endif // !PHYSICS_PHYSICS_MANAGER_H
