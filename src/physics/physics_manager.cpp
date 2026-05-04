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

	return entityRayHit;
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

glm::vec3 PhysicsManager::getVelocity(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getVelocity(handle);
}

float PhysicsManager::getGravityScale(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getGravityScale(handle);
}

uint32_t PhysicsManager::getForceLayers(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return {};

	return world.getForceLayers(handle);
}

ShapeHandle PhysicsManager::getShapeHandle(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return InvalidShapeHandle;

	return world.getShapeHandle(handle);
}

IShape* PhysicsManager::getShape(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return nullptr;

	return world.getShape(handle);
}

void PhysicsManager::setVelocity(EntityID e, glm::vec3 velocity)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setVelocity(handle, velocity);
}

void PhysicsManager::addVelocity(EntityID e, glm::vec3 velocity)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.addVelocity(handle, velocity);
}

void PhysicsManager::setGravityScale(EntityID e, float scale)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setGravityScale(handle, scale);
}

void PhysicsManager::setForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setForceLayers(handle, layers);
}

void PhysicsManager::addForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.addForceLayers(handle, layers);
}

void PhysicsManager::removeForceLayers(EntityID e, uint32_t layers)
{
	RigidBodyHandle handle = getHandle(e);
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

PhysicsCharacter* PhysicsManager::getCharacter(EntityID entity)
{
	RigidBodyHandle handle = getHandle(entity);
	if (!handle.isValid()) return nullptr;

	return world.getCharacter(handle);
}

void PhysicsManager::setCharacterInfo(EntityID entity, const PhysicsCharacterInfo& info)
{
	RigidBodyHandle handle = getHandle(entity);
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
	world.addForce(handle, gen);
}

void PhysicsManager::removeForce(EntityID entity, IForceGenerator* gen)
{
	world.removeForce(entityToHandle.at(entity), gen);
}

void PhysicsManager::removeForce(RigidBodyHandle handle, IForceGenerator* gen)
{
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

float PhysicsManager::getCharacterMaxWalkableAngle(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);

	return world.getCharacterMaxWalkableAngle(handle);
}

void PhysicsManager::setCharacterMaxWalkableAngle(EntityID e, float maxWalkableAngle)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setCharacterMaxWalkableAngle(handle, maxWalkableAngle);
}

float PhysicsManager::getCharacterStepHeight(EntityID e)
{
	RigidBodyHandle handle = getHandle(e);

	return world.getCharacterMaxWalkableAngle(handle);
}

void PhysicsManager::setCharacterStepHeight(EntityID e, float stepHeight)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setCharacterStepHeight(handle, stepHeight);
}

EntityRaycastHit PhysicsManager::raycast(const Ray& ray, const QueryFilterExternal& filter) const
{
	RaycastHit rayHit = world.raycast(ray, getQueryFilterFromQueryFilterExternal(filter));

	return rayHitToEntityRayHit(rayHit);	
}

std::vector<EntityRaycastHit> PhysicsManager::raycastAll(const Ray& ray, const QueryFilterExternal& filter) const
{
	std::vector<EntityRaycastHit> entityRayHits;

	std::vector<RaycastHit> rayHits = world.raycastAll(ray, getQueryFilterFromQueryFilterExternal(filter));
	entityRayHits.resize(rayHits.size());

	for (size_t i = 0; i < rayHits.size(); i++)
	{
		entityRayHits[i] = rayHitToEntityRayHit(rayHits[i]);
	}

	return entityRayHits;
}

std::vector<EntityID> PhysicsManager::shapeIntersects(ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation, const QueryFilterExternal& filter) const
{
	std::vector<EntityID> entityShapeHits;

	std::vector<RigidBodyHandle> shapeHits = world.shapeIntersects(shape, position, orientation, getQueryFilterFromQueryFilterExternal(filter));
	entityShapeHits.resize(shapeHits.size());

	for (size_t i = 0; i < shapeHits.size(); i++)
	{
		entityShapeHits[i] = getEntityID(shapeHits[i]);
	}

	return entityShapeHits;
}
