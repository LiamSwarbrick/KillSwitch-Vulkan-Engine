#ifndef PHYSICS_PHYSICS_MANAGER_H
#define PHYSICS_PHYSICS_MANAGER_H

#include "core/ecs.h"
#include "physics/core/types.h"
#include "physics_world.h"

#include "physics/simulation/force_generators.h"
#include "physics/collision/narrowphase/contact.h"

#include "glm/glm.hpp"

#include <unordered_map>

struct EntityRaycastHit
{
	EntityID entity = NULL_ENTITY;
	glm::vec3 point;
	glm::vec3 normal;
	float t = -1.0f;

	bool isValid() const { return entity != NULL_ENTITY; }
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
	// do not confuse with setShape(ShapeHandle handle, ShapeDesc);
	void setBodyShape(EntityID e, ShapeHandle handle);

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
	glm::vec3 getVelocity(EntityID e);
	float getGravityScale(EntityID e);
	uint32_t getForceLayers(EntityID e);
	ShapeHandle getShapeHandle(EntityID e);
	IShape* getShape(EntityID e); // extra

	void setVelocity(EntityID e, glm::vec3 velocity);
	void addVelocity(EntityID e, glm::vec3 velocity);
	void setGravityScale(EntityID e, float scale);
	void setForceLayers(EntityID e, uint32_t layers);
	void addForceLayers(EntityID e, uint32_t layers);
	void removeForceLayers(EntityID e, uint32_t layers);
	


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
	
	EntityRaycastHit raycast(const Ray& ray, const QueryFilter& filter = {}) const;

	std::vector<EntityRaycastHit> raycastAll(const Ray& ray, const QueryFilter& filter = {}) const;

	// Shape-casting too (might change the input to be ShapeCast or something like that, but for now this, will see when i implement it)
	std::vector<EntityID> shapecast(
		ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation, 
		const QueryFilter& filter = {}) const;

	// ------------------------------
	// EVENTS (planning for the future)
	// ------------------------------
	std::function<void(EntityID a, EntityID b, const Contact&)> onCollisionEnter;
	std::function<void(EntityID a, EntityID b, const Contact&)> onCollisionStay;
	std::function<void(EntityID a, EntityID b)> onCollisionExit;

	std::function<void(EntityID a, EntityID b)> onTriggerEnter;
	std::function<void(EntityID a, EntityID b)> onTriggerStay;
	std::function<void(EntityID a, EntityID b)> onTriggerExit;


private:
	inline RigidBodyHandle getHandle(EntityID entity);
	inline EntityID getEntityID(RigidBodyHandle handle);

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
	//void bindWorldEvents();
};

#endif // !PHYSICS_PHYSICS_MANAGER_H
