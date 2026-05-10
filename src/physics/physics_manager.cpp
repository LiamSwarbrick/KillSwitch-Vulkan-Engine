#include "physics_manager.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "core/components.h"
#include "physics/components.h"

#include<span>

void PhysicsManager::startUp()
{
	bindWorldEvents();
}

void PhysicsManager::shutDown()
{
	clear();
}

void PhysicsManager::clear()
{
	entityToHandle.clear();
	handleToEntity.clear();
	world.clear();
}

RigidBodyHandle PhysicsManager::createBody(EntityID entity, const RigidBodyDesc& desc)
{
	auto it = entityToHandle.find(entity);
	if (it != entityToHandle.end())
	{
		assert(false && "Entity already has rigidbody");
	}
	

	RigidBodyHandle handle = world.addBody(desc);

	if (handle.isValid())
	{
		entityToHandle.insert({ entity, handle });
		handleToEntity.insert({ handle, entity });
	}

	return handle;
}

void PhysicsManager::destroyBody(EntityID entity)
{
	RigidBodyHandle handle = getHandle(entity);
	if (!handle.isValid()) return;

	world.removeBody(handle);

	entityToHandle.erase(entity);
	handleToEntity.erase(handle);
}

void PhysicsManager::destroyBody(RigidBodyHandle handle)
{
	if (!handle.isValid()) return;

	EntityID entity = getEntityID(handle);

	world.removeBody(handle);

	entityToHandle.erase(entity);
	handleToEntity.erase(handle);
}

ShapeHandle PhysicsManager::createShape(const ShapeDesc& desc)
{
	return world.createShape(desc);
}

void PhysicsManager::destroyShape(ShapeHandle handle)
{
	world.destroyShape(handle);
}



inline RigidBodyHandle PhysicsManager::getHandle(EntityID entity) const
{
	// Can fail but shouldn't if entities are managed properly!
	auto it = entityToHandle.find(entity);
	
	if (it == entityToHandle.end())
	{
		SDL_assert(false && "Invalid EntityID to RigidBodyHandle");
		return InvalidRigidBodyHandle;
	}

	return it->second;
}

inline EntityID PhysicsManager::getEntityID(RigidBodyHandle handle) const
{
	// Should never fail !!!
	auto it = handleToEntity.find(handle);

	if (it == handleToEntity.end())
	{
		SDL_assert(false && "Invalid RigidBodyHandle to EntityID");
		return NULL_ENTITY;
	}

	return it->second;
}

inline QueryFilter PhysicsManager::getQueryFilterFromQueryFilterExternal(const QueryFilterExternal& queryFilterExternal) const
{
	QueryFilter res;
	if (queryFilterExternal.bodyToIgnore != NULL_ENTITY) res.bodyToIgnore = getHandle(queryFilterExternal.bodyToIgnore);
	res.hasLayerOfQuery = queryFilterExternal.hasLayerOfQuery;
	res.layerOfQuery = queryFilterExternal.layerOfQuery;

	return res;
}

void PhysicsManager::bindWorldEvents()
{
	world.onCollisionEnter = [this](RigidBodyHandle handleA, RigidBodyHandle handleB, const Contact& c)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onCollisionEnter.Invoke({ a, b, c });
		};

	world.onCollisionStay = [this](RigidBodyHandle handleA, RigidBodyHandle handleB, const Contact& c)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onCollisionStay.Invoke({ a, b, c });
		};

	world.onCollisionExit = [this](RigidBodyHandle handleA, RigidBodyHandle handleB)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onCollisionExit.Invoke({ a, b });
		};

	world.onTriggerEnter = [this](RigidBodyHandle handleA, RigidBodyHandle handleB, const Contact& c)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onTriggerEnter.Invoke({ a, b, c });
		};

	world.onTriggerStay = [this](RigidBodyHandle handleA, RigidBodyHandle handleB, const Contact& c)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onTriggerStay.Invoke({ a, b, c });
		};

	world.onTriggerExit = [this](RigidBodyHandle handleA, RigidBodyHandle handleB)
		{
			EntityID a = getEntityID(handleA);
			EntityID b = getEntityID(handleB);

			if (a != NULL_ENTITY && b != NULL_ENTITY)
				onTriggerExit.Invoke({ a, b });
		};

}

inline EntityRaycastHit PhysicsManager::rayHitToEntityRayHit(const RaycastHit& rayHit) const
{
	if (!rayHit.body || !rayHit.isValid()) return { /* Default EntityRaycastHit */ };

	EntityRaycastHit entityRayHit;

	entityRayHit.point = rayHit.point;
	entityRayHit.normal = rayHit.normal;
	entityRayHit.t = rayHit.t;

	entityRayHit.entity = getEntityID({ rayHit.body->bodyID });
	entityRayHit.body = rayHit.body;

	return entityRayHit;
}

inline EntityShapecastHit PhysicsManager::shapeHitToEntityShapeHit(const ShapecastHit& shapeHit) const
{
	if (!shapeHit.body || !shapeHit.isValid()) return EntityShapecastHit::none();
	EntityShapecastHit entityShapeHit;

	entityShapeHit.point = shapeHit.point;
	entityShapeHit.pointA = shapeHit.pointA;
	entityShapeHit.pointB = shapeHit.pointB;
	entityShapeHit.t = shapeHit.t;
	entityShapeHit.normal  = shapeHit.normal;

	entityShapeHit.entity = getEntityID({ shapeHit.body->bodyID });
	entityShapeHit.body = shapeHit.body;

	return entityShapeHit;
}


const RigidBody* PhysicsManager::getBody(EntityID entity)
{
	RigidBodyHandle handle = getHandle(entity);
	return world.getBody(handle);
}

const RigidBody* PhysicsManager::getBody(RigidBodyHandle entity)
{
	RigidBodyHandle handle = getHandle(entity);
	return world.getBody(handle);
}

void PhysicsManager::update(ECS& ecs, float dt)
{
	// Sync data (transforms) from ECS to the PhysicsWorld
	auto packed = ecs.GetView<C_Transform, C_RigidBody>().GetPacked();
	std::vector<PhysicsWorld::TransformData> transforms;
	transforms.reserve(packed.size());

	for (size_t i = 0; i < packed.size(); i++)
	{
		auto [t, rb] = packed[i].components;

		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(t.matrix, scale, rotation, translation, skew, perspective);

		transforms.push_back({ translation, rotation, rb.handle });
	}

	world.syncTransformsIn(transforms);
	world.update(dt);
	world.syncTransformsOut(transforms); // will modify transforms back

	// Sync data (transforms) back to ECS
	for (size_t i = 0; i < packed.size(); i++)
	{
		auto [t, rb] = packed[i].components;
		PhysicsWorld::TransformData outData = transforms[i];
		/*t.position = outData.position;
		t.rotation = outData.orientation;*/

		t.matrix = glm::translate(glm::mat4(1.0f), outData.position) * glm::mat4(outData.orientation);
	}
}

glm::vec3 PhysicsManager::getWorldUp()
{
	return world.getWorldUp();
}

glm::vec3 PhysicsManager::getVelocity(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getVelocity(handle);
}

glm::vec3 PhysicsManager::getVelocity(RigidBodyHandle handle)
{
	if (!handle.isValid()) return {};

	return world.getVelocity(handle);
}

float PhysicsManager::getGravityScale(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getGravityScale(handle);
}

float PhysicsManager::getGravityScale(RigidBodyHandle handle)
{
	if (!handle.isValid()) return {};

	return world.getGravityScale(handle);
}

uint32_t PhysicsManager::getForceLayers(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getForceLayers(handle);
}

uint32_t PhysicsManager::getForceLayers(RigidBodyHandle handle)
{
	if (!handle.isValid()) return {};

	return world.getForceLayers(handle);
}

ShapeHandle PhysicsManager::getShapeHandle(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return InvalidShapeHandle;

	return world.getShapeHandle(handle);
}

ShapeHandle PhysicsManager::getShapeHandle(RigidBodyHandle handle)
{
	if (!handle.isValid()) return InvalidShapeHandle;

	return world.getShapeHandle(handle);
}

Shape* PhysicsManager::getShape(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return nullptr;

	return world.getShape(handle);
}

Shape* PhysicsManager::getShape(RigidBodyHandle handle)
{
	if (!handle.isValid()) return nullptr;

	return world.getShape(handle);
}

void PhysicsManager::teleportBody(EntityID e, const glm::vec3& worldPosition)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.teleportBody(handle, worldPosition);
}

void PhysicsManager::teleportBody(RigidBodyHandle handle, const glm::vec3& worldPosition)
{
	if (!handle.isValid()) return;
	world.teleportBody(handle, worldPosition);
}

void PhysicsManager::setVelocity(EntityID e, const glm::vec3& velocity)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setVelocity(handle, velocity);
}

void PhysicsManager::setVelocity(RigidBodyHandle handle, const glm::vec3& velocity)
{
	if (!handle.isValid()) return;
	world.setVelocity(handle, velocity);
}

void PhysicsManager::addVelocity(EntityID e, const glm::vec3& velocity)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.addVelocity(handle, velocity);
}

void PhysicsManager::addVelocity(RigidBodyHandle handle, const glm::vec3& velocity)
{
	if (!handle.isValid()) return;
	world.addVelocity(handle, velocity);
}

void PhysicsManager::setGravityScale(EntityID e, float scale)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setGravityScale(handle, scale);
}

void PhysicsManager::setGravityScale(RigidBodyHandle handle, float scale)
{
	if (!handle.isValid()) return;
	world.setGravityScale(handle, scale);
}

void PhysicsManager::setForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setForceLayers(handle, layers);
}

void PhysicsManager::setForceLayers(RigidBodyHandle handle, uint32_t layers)
{

	if (!handle.isValid()) return;

	world.setForceLayers(handle, layers);
}

void PhysicsManager::addForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.addForceLayers(handle, layers);
}

void PhysicsManager::addForceLayers(RigidBodyHandle handle, uint32_t layers)
{
	if (!handle.isValid()) return;

	world.addForceLayers(handle, layers);
}

void PhysicsManager::removeForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.removeForceLayers(handle, layers);
}

void PhysicsManager::removeForceLayers(RigidBodyHandle handle, uint32_t layers)
{
	if (!handle.isValid()) return;

	world.removeForceLayers(handle, layers);
}

glm::vec3 PhysicsManager::getGravity() const
{
	return world.getGravity();
}

void PhysicsManager::setBodyShape(EntityID e, ShapeHandle shapeHandle)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setBodyShape(handle, shapeHandle);
}

void PhysicsManager::setBodyShape(RigidBodyHandle handle, ShapeHandle shapeHandle)
{
	if (!handle.isValid()) return;

	world.setBodyShape(handle, shapeHandle);
}

PhysicsCharacter* PhysicsManager::getCharacter(EntityID entity)
{
	RigidBodyHandle handle = getHandle(entity);
	if (!handle.isValid()) return nullptr;

	return world.getCharacter(handle);
}

PhysicsCharacter* PhysicsManager::getCharacter(RigidBodyHandle handle)
{
	if (!handle.isValid()) return nullptr;

	return world.getCharacter(handle);
}

void PhysicsManager::setCharacterInfo(EntityID entity, const PhysicsCharacterInfo& info)
{
	RigidBodyHandle handle = getHandle(entity);
	if (!handle.isValid()) return;

	world.setCharacterInfo(handle, info);
}

void PhysicsManager::setCharacterInfo(RigidBodyHandle handle, const PhysicsCharacterInfo& info)
{
	if (!handle.isValid()) return;

	world.setCharacterInfo(handle, info);
}


// ------------------------------
// FORCES (REGISTRY)
// ------------------------------
void PhysicsManager::addGenerator(IForceGenerator* gen)
{
	world.addGenerator(gen);
}

void PhysicsManager::removeGenerator(IForceGenerator* gen)
{
	world.removeGenerator(gen);
}

void PhysicsManager::addForce(EntityID entity, IForceGenerator* gen)
{
	RigidBodyHandle handle = getHandle(entity);
	if (!handle.isValid()) return;

	world.addForce(handle, gen);
}

void PhysicsManager::addForce(RigidBodyHandle handle, IForceGenerator* gen)
{
	if (!handle.isValid()) return;

	world.addForce(handle, gen);
}

void PhysicsManager::removeForce(EntityID entity, IForceGenerator* gen)
{
	world.removeForce(entityToHandle.at(entity), gen);
}

void PhysicsManager::removeForce(RigidBodyHandle handle, IForceGenerator* gen)
{
	if (!handle.isValid()) return;

	world.removeForce(handle, gen);
}

void PhysicsManager::setNumLayers(uint8_t numLayers)
{
	world.setNumLayers(numLayers);
}

void PhysicsManager::setLayerPair(uint8_t a, uint8_t b, bool shouldCollide)
{
	world.setLayerPair(a, b, shouldCollide);
}

void PhysicsManager::enableLayerPair(uint8_t a, uint8_t b)
{
	world.enableLayerPair(a, b);
}

void PhysicsManager::disableLayerPair(uint8_t a, uint8_t b)
{
	world.disableLayerPair(a, b);
}

PhysicsCharacter::GroundState PhysicsManager::getCharacterGroundState(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);

	return world.getCharacterGroundState(handle);
}

PhysicsCharacter::GroundState PhysicsManager::getCharacterGroundState(RigidBodyHandle handle)
{
	return world.getCharacterGroundState(handle);
}

float PhysicsManager::getCharacterMaxWalkableAngle(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);

	return world.getCharacterMaxWalkableAngle(handle);
}

float PhysicsManager::getCharacterMaxWalkableAngle(RigidBodyHandle handle)
{
	return world.getCharacterMaxWalkableAngle(handle);
}

void PhysicsManager::setCharacterMaxWalkableAngle(EntityID e, float maxWalkableAngle)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setCharacterMaxWalkableAngle(handle, maxWalkableAngle);
}

void PhysicsManager::setCharacterMaxWalkableAngle(RigidBodyHandle handle, float maxWalkableAngle)
{
	if (!handle.isValid()) return;

	world.setCharacterMaxWalkableAngle(handle, maxWalkableAngle);
}

float PhysicsManager::getCharacterStepHeight(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);

	return world.getCharacterStepHeight(handle);
}

float PhysicsManager::getCharacterStepHeight(RigidBodyHandle handle)
{
	return world.getCharacterStepHeight(handle);
}

void PhysicsManager::setCharacterStepHeight(EntityID e, float stepHeight)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setCharacterStepHeight(handle, stepHeight);
}

void PhysicsManager::setCharacterStepHeight(RigidBodyHandle handle, float stepHeight)
{
	if (!handle.isValid()) return;

	world.setCharacterStepHeight(handle, stepHeight);
}

EntityRaycastHit PhysicsManager::raycast(const Ray& ray, const QueryFilterExternal& filter) const
{
	return raycast(ray, getQueryFilterFromQueryFilterExternal(filter));
}

EntityRaycastHit PhysicsManager::raycast(const Ray& ray, const QueryFilter& filter) const
{
	RaycastHit rayHit = world.raycast(ray, filter);

	return rayHitToEntityRayHit(rayHit);
}


std::vector<EntityRaycastHit> PhysicsManager::raycastAll(const Ray& ray, const QueryFilterExternal& filter) const
{
	return raycastAll(ray, getQueryFilterFromQueryFilterExternal(filter));
}

std::vector<EntityRaycastHit> PhysicsManager::raycastAll(const Ray& ray, const QueryFilter& filter) const
{
	std::vector<EntityRaycastHit> entityRayHits;

	std::vector<RaycastHit> rayHits = world.raycastAll(ray, filter);
	entityRayHits.resize(rayHits.size());

	for (size_t i = 0; i < rayHits.size(); i++)
	{
		entityRayHits[i] = rayHitToEntityRayHit(rayHits[i]);
	}

	return entityRayHits;
}

EntityShapecastHit PhysicsManager::shapecast(const Ray& ray, ShapeHandle shape, const glm::quat& orientation, const QueryFilterExternal& filter) const
{
	return shapecast(ray, shape, orientation, getQueryFilterFromQueryFilterExternal(filter));
}

EntityShapecastHit PhysicsManager::shapecast(const Ray& ray, ShapeHandle shape, const glm::quat& orientation, const QueryFilter& filter) const
{
	ShapecastHit hit = world.shapecast(ray, shape, orientation, filter);

	return shapeHitToEntityShapeHit(hit);
}

std::vector<EntityShapeIntersectsHit> PhysicsManager::shapeIntersects(ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation, const QueryFilterExternal& filter) const
{
	return shapeIntersects(shape, position, orientation, getQueryFilterFromQueryFilterExternal(filter));
}

std::vector<EntityShapeIntersectsHit> PhysicsManager::shapeIntersects(ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation, const QueryFilter& filter) const
{
	std::vector<EntityShapeIntersectsHit> entityShapeHits;

	std::vector<RigidBodyHandle> shapeHits = world.shapeIntersects(shape, position, orientation, filter);
	entityShapeHits.resize(shapeHits.size());

	for (size_t i = 0; i < shapeHits.size(); i++)
	{
		entityShapeHits[i] = {
			.entity = getEntityID(shapeHits[i]),
			.body = shapeHits[i]
		};
	}

	return entityShapeHits;
}
