#include "physics_world.h"

#include "shapes/sphere.h"
#include "shapes/capsule.h"
#include "shapes/box.h"
#include "shapes/plane.h"

#include "SDL3/SDL.h"
#include <vector>
#include "physics_manager.h"

PhysicsWorld::PhysicsWorld(uint32_t expectedBodies)
{
	bodies.Reserve(expectedBodies);
	shapes.Reserve(expectedBodies);
	planes.Reserve(100);
	
	contacts.reserve(expectedBodies * 2);

	// add the gravity to the generator
	gravityGen.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
	addGenerator(&gravityGen);
}

PhysicsWorld::~PhysicsWorld()
{
	clear();
}

void PhysicsWorld::clear()
{
	bodies.Clear();
	clearShapeRefs();
	planes.Clear();
}

void PhysicsWorld::clearShapeRefs()
{
	for (ShapeRef& s : shapes.Data())
	{
		delete s.shape;
		s.shape = nullptr;
	}
	shapes.Clear();
}

void PhysicsWorld::syncTransformsIn(std::span<const TransformData> transforms)
{
	for (const TransformData& transform : transforms)
	{
		RigidBody* body = getBody(transform.handle);
		// skip if the body is static
		if (body==nullptr || body->isStatic)
			continue;

		body->position = transform.position;
		body->orientation = transform.orientation;

		//calculateAABB(body);
	}
}

void PhysicsWorld::syncTransformsOut(std::span<TransformData> outTransforms)
{
	for (TransformData& transform : outTransforms)
	{
		const RigidBody* body = getBody(transform.handle);
		if (body == nullptr || body->isStatic)
			continue;

		transform.position = body->position;
		transform.orientation = body->orientation;
	}
}

void PhysicsWorld::update(float dt)
{
	stepAccumulator += dt;
	int steps = 0;

	while (stepAccumulator >= fixedStep && steps < maxSteps)
	{
		step(fixedStep);
		stepAccumulator -= fixedStep;
		steps++;
	}

	if (steps == maxSteps)
		stepAccumulator = 0.0f;
}

glm::vec3 PhysicsWorld::getVelocity(RigidBodyHandle r)
{
	RigidBody* b = getBody(r);
	return b->velocity;
}

float PhysicsWorld::getGravityScale(RigidBodyHandle r)
{
	RigidBody* b = getBody(r);
	return b->gravityScale;
}

uint32_t PhysicsWorld::getForceLayers(RigidBodyHandle r)
{
	RigidBody* b = getBody(r);
	return b->forceLayers;
}

IShape* PhysicsWorld::getShape(RigidBodyHandle r)
{
	RigidBody* b = getBody(r);
	return getShape(b->shapeHandle);
}

ShapeHandle PhysicsWorld::getShapeHandle(RigidBodyHandle r)
{
	RigidBody* b = getBody(r);
	if (!b) return InvalidShapeHandle;
	return b->shapeHandle;
}

void PhysicsWorld::setVelocity(RigidBodyHandle r, glm::vec3 velocity)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->velocity = velocity;
}

void PhysicsWorld::addVelocity(RigidBodyHandle r, glm::vec3 velocity)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->velocity += velocity;
}

void PhysicsWorld::setGravityScale(RigidBodyHandle r, float scale)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->gravityScale = scale;
}

void PhysicsWorld::setForceLayers(RigidBodyHandle r, uint32_t layers)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->forceLayers = layers;
}

void PhysicsWorld::addForceLayers(RigidBodyHandle r, uint32_t layers)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->forceLayers |= layers;
}

void PhysicsWorld::removeForceLayers(RigidBodyHandle r, uint32_t layers)
{
	RigidBody* b = getBody(r);
	b->forceLayers &= ~layers;
}


RigidBodyHandle PhysicsWorld::addBody(const RigidBodyDesc& desc)
{
	SDL_assert(desc.shape.isValid() && "RigidBodyDesc must have a valid ShapeHandle");
	// We should check the mass though, but we could make it unsigned, i'll leave it like this

	// Check the index
	uint32_t index;
	// If we don't have recyclable entities
	if (freeBodyIndices.size() == 0)
	{
		if (bodies.Size() == UINT32_MAX)
		{
			// SDL_ log (not logarithm) ("Max number of entities reached");
			SDL_assert(false && "RigidBody count has reached its limit, can't insert more");
		}

		// Get the next index (so bodies.size());
		index = bodies.Size();
	}
	else
	{
		// Recycle index
		index = freeBodyIndices.back();
		freeBodyIndices.pop_back();
	}

	// Build the body based on the descriptor
	RigidBody body;

	body.position = desc.position;
	body.orientation = desc.orientation; 
	body.velocity = glm::vec3(0.0f);
	body.forceAccumulator = glm::vec3(0.0f);
	body.mass = desc.mass;
	body.invMass = desc.isStatic ? 0.0f : (1.0f / desc.mass);
	body.gravityScale = desc.gravityScale;
	body.damping = desc.damping;
	body.forceLayers = desc.forceLayers;

	body.shapeHandle = desc.shape;

	body.isStatic = desc.isStatic;
	body.isKinematic = desc.isKinematic;
	body.isCharacter = desc.isCharacter;
	body.isTrigger = desc.isTrigger;

	// vERY IMPORTANT
	body.bodyID = index;

	// Unused as we are not putting bodies to sleep for now
	body.sleeping = false;

	calculateAABB(&body);

	// Retain shape ref
	retainShape(desc.shape);

	bodies.Set(index, std::move(body));

	// TODO: reminder to insert and remove into broadphase
	broadPhase.insert(bodies.GetPtr(index));


	return RigidBodyHandle{ index };
}

void PhysicsWorld::removeBody(RigidBodyHandle handle)
{
	SDL_assert(handle.isValid() && "Trying to remove a body with invalid handle");

	RigidBody* body = bodies.GetPtr(handle.index);

	// remove from broad-phase
	broadPhase.remove(body);
	// remove from forceRegistry
	forceRegistry.removeAll(body);
	// remove body from the previous pairs, not to take them into account for next frame
	std::erase_if(previousCollisionPairs, [body](const BodyPair& pair)
		{
			return pair.bodyA == body || pair.bodyB == body;
		});
	std::erase_if(previousTriggerPairs, [body](const BodyPair& pair)
		{
			return pair.bodyA == body || pair.bodyB == body;
		});

	// release shape ref before deleting
	releaseShape(body->shapeHandle);

	// SparseSet.Delete() has internal check of a handle.
	bodies.Delete(handle.index);
	freeBodyIndices.push_back(handle.index);
}

RigidBody* PhysicsWorld::getBody(RigidBodyHandle handle)
{
	SDL_assert(handle.isValid() && "BodyHandle is invalid");
	if (!handle.isValid()) return nullptr;

	return bodies.GetPtr(handle.index);
}

const RigidBody* PhysicsWorld::getBody(RigidBodyHandle handle) const
{
	SDL_assert(handle.isValid() && "BodyHandle is invalid");
	if (!handle.isValid()) return nullptr;

	return bodies.GetPtr(handle.index);
}

void PhysicsWorld::setBodyShape(RigidBodyHandle bodyHandle, ShapeHandle shapeHandle)
{
	RigidBody* body = bodies.GetPtr(bodyHandle.index);
	IShape* shape = shapes.GetPtr(shapeHandle.index)->shape;
	if (!body || !shape) return;

	// Doing retain first just in case our newHandle is the oldHandle
	// So we avoid automatic deletion and then retaining. It would bug otherwise
	retainShape(shapeHandle);
	releaseShape(body->shapeHandle);
	body->shapeHandle = shapeHandle;

	broadPhase.remove(body);
	calculateAABB(body);
	broadPhase.insert(body);
}

ShapeHandle PhysicsWorld::createShape(const ShapeDesc& desc)
{
	// Check the index
	uint32_t index;
	// If we don't have recyclable entities
	if (freeShapeIndices.size() == 0)
	{
		if (shapes.Size() == UINT32_MAX)
		{
			// SDL_ log (not logarithm) ("Max number of entities reached");
			SDL_assert(false && "Shapes count has reached its limit, can't insert more");
		}

		// Get the next index (so bodies.size());
		index = shapes.Size();
	}
	else
	{
		// Recycle index
		index = freeShapeIndices.back();
		freeShapeIndices.pop_back();
	}

	ShapeRef ref;
	ref.refCount = 0;
	switch (desc.type)
	{
	case ShapeType::Sphere:
		ref.shape = new SphereShape(desc.sphere.radius);
		break;
	case ShapeType::Box:
		ref.shape = new BoxShape(desc.box.halfWidths);
		break;
	case ShapeType::Capsule:
		ref.shape = new CapsuleShape(desc.capsule.radius, desc.capsule.halfHeight);
		break;
	case ShapeType::Plane:
		SDL_assert(false && "CreateShape doesn't support ShapeType::Plane, use addPlane(desc)");
		break;
	default:
		SDL_assert(false && "Unknown ShapeType in PhysicsWorld::createShape");
		break;
	}

	// don't forget to add the offset (even thought i might delete these 2 parameters later)
	ref.shape->localOffset = desc.localOffset;
	ref.shape->localOrientation = desc.localOrientation;

	shapes.Set(index, std::move(ref));

	return { index };
}

inline PhysicsWorld::ShapeRef* PhysicsWorld::getShapeRef(ShapeHandle handle)
{
	return shapes.GetPtr(handle.index);
}

void PhysicsWorld::retainShape(ShapeHandle handle)
{
	ShapeRef* ref = getShapeRef(handle);
	ref->refCount++;
}

void PhysicsWorld::releaseShape(ShapeHandle handle)
{
	ShapeRef* ref = getShapeRef(handle);

	SDL_assert(ref && "Invalid ShapeRef");
	SDL_assert(ref->refCount > 0 && "Releasing a shape with 0 or less references");
	if (!ref) return;

	ref->refCount--;

	if (ref->refCount == 0)
	{
		destroyShape(handle);
	}
}

void PhysicsWorld::destroyShape(ShapeHandle handle)
{
	SDL_assert(handle.isValid() && "Trying to remove a shape with invalid handle");
	
	ShapeRef* ref = getShapeRef(handle);
	if (ref && ref->refCount > 0)
		SDL_assert(false && "Destroying a shape with dangling references");

	// delete the pointer before removing ShapeRef
	delete ref->shape;
	ref->shape = nullptr;

	shapes.Delete(handle.index);
	freeShapeIndices.push_back(handle.index);
}

IShape* PhysicsWorld::getShape(ShapeHandle handle)
{
	if (!handle.isValid()) return nullptr;

	return shapes.GetPtr(handle.index)->shape;
}

const IShape* PhysicsWorld::getShape(ShapeHandle handle) const
{
	if (!handle.isValid()) return nullptr;

	return shapes.GetPtr(handle.index)->shape;
}


PlaneHandle PhysicsWorld::addPlane(const ShapeDesc& desc)
{
	SDL_assert(desc.type == ShapeType::Plane && "ShapeDesc in addPlane should have type::Plane");
	// Check the index
	uint32_t index;
	// If we don't have recyclable entities
	if (freePlaneIndices.size() == 0)
	{
		if (planes.Size() == UINT32_MAX)
		{
			// SDL_ log (not logarithm) ("Max number of entities reached");
			SDL_assert(false && "Planes count has reached its limit, can't insert more");
		}

		// Get the next index (so bodies.size());
		index = shapes.Size();
	}
	else
	{
		// Recycle index
		index = freePlaneIndices.back();
		freePlaneIndices.pop_back();
	}

	if (desc.type != ShapeType::Plane) return InvalidPlaneHandle;
	PlaneShape plane(desc.plane.normal, desc.plane.distance);

	planes.Set(index, plane);

	return { index };
}

void PhysicsWorld::removePlane(PlaneHandle handle)
{
	SDL_assert(handle.isValid() && "Invalid handle on removePlane");

	planes.Delete(handle.index);
}

void PhysicsWorld::addGenerator(IForceGenerator* gen)
{
	forceRegistry.addGenerator(gen);
}

void PhysicsWorld::removeGenerator(IForceGenerator* gen)
{
	forceRegistry.removeGenerator(gen);
}

void PhysicsWorld::addForce(RigidBodyHandle handle, IForceGenerator* gen)
{
	RigidBody* body = getBody(handle);
	if (body)
		forceRegistry.addPair(body, gen);
}

void PhysicsWorld::removeForce(RigidBodyHandle handle, IForceGenerator* gen)
{
	RigidBody* body = getBody(handle);
	if (body)
		forceRegistry.removePair(body, gen);
}

void PhysicsWorld::setGravity(glm::vec3 g)
{
	gravityGen.gravity = g;
}

glm::vec3 PhysicsWorld::getGravity() const
{
	return gravityGen.gravity;
}

void PhysicsWorld::setFixedStep(float step)
{
	fixedStep = step;
}

float PhysicsWorld::getFixedStep() const
{
	return fixedStep;
}

void PhysicsWorld::setMaxSteps(int max)
{
	maxSteps = max;
}

int PhysicsWorld::getMaxSteps() const
{
	return maxSteps;
}

void PhysicsWorld::step(float dt)
{
	applyForces(dt);
	integrate(dt);
	updateBroadPhase();
	detectCollisions();
	testPlanes();
	solve(dt);
}

void PhysicsWorld::applyForces(float dt)
{
	forceRegistry.applyAll(bodies.Data(), dt);
}

void PhysicsWorld::integrate(float dt)
{
	for (RigidBody& body : bodies.Data())
	{
		integrator.integrate(body, dt);
	}
}

void PhysicsWorld::updateBroadPhase()
{
	// Updating here cause we want the broadphase to NOT care about the bodies's shapes
	// Just the AABBs

	for (RigidBody& body : bodies.Data())
	{
		if (body.isStatic) continue;

		// Not calling calculateAABB(&body) because we're not fattening the newAABB
		IShape* shape = getShape(body.shapeHandle);
		if (!shape) continue;

		glm::vec3 shapePosition;
		glm::quat shapeOrientation;
		narrowPhase.resolveShapeTransform(shape,
			body.position, body.orientation,
			shapePosition, shapeOrientation);

		AABB newAABB = shape->computeAABB(shapePosition, shapeOrientation);
	
		// if the new AABB exceeds the bounds of the past AABB
		if (!body.aabb.contains(newAABB))
		{
			broadPhase.remove(&body);
			body.aabb = newAABB.fattened(0.1f);
			broadPhase.insert(&body);
		}
	}
}

void PhysicsWorld::detectCollisions()
{
	contacts.clear();
	
	std::vector<BodyPair> pairs;

	broadPhase.queryPairs(pairs);

	for (const BodyPair& pair : pairs)
	{
		if (pair.bodyA->isStatic && pair.bodyB->isStatic) continue;
		if (pair.bodyA->isKinematic && pair.bodyB->isKinematic) continue; // this should be the case right????

		Contact contact = narrowPhase.testPair(*pair.bodyA, *pair.bodyB, *this);
		if (contact.isValid())
			contacts.push_back(contact);
	}
}

void PhysicsWorld::testPlanes()
{
	for (const PlaneShape& plane : planes.Data())
	{
		for (const RigidBody& body : bodies.Data())
		{
			if (body.isStatic) continue;

			Contact contact = narrowPhase.testPlane(body, plane, *this);
			if (contact.isValid())
				contacts.push_back(contact);
		}
	}
}

void PhysicsWorld::solve(float dt)
{
	solver.solve(contacts, dt);
}


void PhysicsWorld::calculateAABB(RigidBody* body)
{
	IShape* shape = getShape(body->shapeHandle);
	if (shape)
	{
		glm::vec3 shapePosition;
		glm::quat shapeOrientation;
		narrowPhase.resolveShapeTransform(shape,
			body->position, body->orientation,
			shapePosition, shapeOrientation);
		body->aabb = shape->computeAABB(shapePosition, shapeOrientation).fattened(0.1f);
	}
}
