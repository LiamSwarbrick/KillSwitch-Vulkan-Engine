#include "physics_manager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/components.h"
#include "physics/components.h"

#include<span>

void PhysicsManager::startUp()
{
	//bindWorldEvents();
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
	assert(entityToHandle.find(entity) == entityToHandle.end() && "Entity already has rigidbody");

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



inline RigidBodyHandle PhysicsManager::getHandle(EntityID entity)
{
	// Can fail but shouldn't if entities are managed properly!
	auto it = entityToHandle.find(entity);
	SDL_assert(it == entityToHandle.end() && "Invalid EntityID to RigidBodyHandle");
	if (it == entityToHandle.end()) return InvalidRigidBodyHandle;

	return it->second;
}

inline EntityID PhysicsManager::getEntityID(RigidBodyHandle handle)
{
	// Should never fail !!!
	auto it = handleToEntity.find(handle);
	SDL_assert(it == handleToEntity.end() && "Invalid RigidBodyHandle to EntityID");
	if (it == handleToEntity.end()) return NULL_ENTITY;

	return it->second;
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

		transforms.push_back({ t.position, t.rotation, rb.handle });
	}

	world.syncTransformsIn(transforms);
	world.update(dt);
	world.syncTransformsOut(transforms); // will modify transforms back

	// Sync data (transforms) back to ECS
	for (size_t i = 0; i < packed.size(); i++)
	{
		auto [t, rb] = packed[i].components;
		PhysicsWorld::TransformData outData = transforms[i];
		t.position = outData.position;
		t.rotation = outData.orientation;

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

void PhysicsManager::setBodyShape(EntityID e, ShapeHandle shapeHandle)
{
	RigidBodyHandle handle = getHandle(e);
	if (!handle.isValid()) return;

	world.setBodyShape(handle, shapeHandle);
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
	world.addForce(entityToHandle.at(entity), gen);
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

EntityRaycastHit PhysicsManager::raycast(const Ray& ray, const QueryFilter& filter) const
{
	
}

std::vector<EntityRaycastHit> PhysicsManager::raycastAll(const Ray& ray, const QueryFilter&) const
{

}

// Shape-casting too (might change the input to be ShapeCast or something like that, but for now this, will see when i implement it)
std::vector<EntityID> PhysicsManager::shapecast(
	ShapeHandle shape, const glm::vec3& position, const glm::quat& orientation,
	const QueryFilter& filter) const
{

}
