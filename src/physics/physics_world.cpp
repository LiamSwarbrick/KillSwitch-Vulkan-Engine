#include "physics_world.h"

#include "shapes/sphere.h"
#include "shapes/capsule.h"
#include "shapes/box.h"
#include "shapes/plane.h"

#include "SDL3/SDL.h"

#include <memory>
#include <vector>
#include <cmath>

PhysicsWorld::PhysicsWorld(uint8_t numBodyLayers, uint32_t expectedBodies)
{
	bodyLayerFilter.StartUp(numBodyLayers);
	broadPhase = BroadPhase(&bodyLayerFilter);

	bodies.Reserve(expectedBodies);
	shapes.Reserve(expectedBodies);
	characters.Reserve(300);
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

void PhysicsWorld::teleportBody(RigidBodyHandle r, const glm::vec3& worldPosition)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	teleportBodyRaw(b, worldPosition);
}

void PhysicsWorld::setVelocity(RigidBodyHandle r, const glm::vec3& velocity)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->wakeUp();
	b->velocity = velocity;
}

void PhysicsWorld::addVelocity(RigidBodyHandle r, const glm::vec3& velocity)
{
	RigidBody* b = getBody(r);
	if (!b) return;
	b->wakeUp();
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

void PhysicsWorld::setNumLayers(uint8_t numLayers)
{
	bodyLayerFilter.setNumLayers(numLayers);
}

void PhysicsWorld::setLayerPair(uint8_t a, uint8_t b, bool shouldCollide)
{
	bodyLayerFilter.setLayerPair(a, b, shouldCollide);
}

void PhysicsWorld::enableLayerPair(uint8_t a, uint8_t b)
{
	bodyLayerFilter.enableLayerPair(a, b);
}

void PhysicsWorld::disableLayerPair(uint8_t a, uint8_t b)
{
	bodyLayerFilter.disableLayerPair(a, b);
}


PhysicsCharacter::GroundState PhysicsWorld::getCharacterGroundState(RigidBodyHandle r)
{
	PhysicsCharacter* c = getCharacter(r);

	if (c == nullptr) return PhysicsCharacter::GroundState::Undefined;

	return c->groundState;
}

float PhysicsWorld::getCharacterMaxWalkableAngle(RigidBodyHandle r)
{
	PhysicsCharacter* c = getCharacter(r);

	if (c == nullptr) return -1.0f;

	return c->maxWalkableAngle;
}

void PhysicsWorld::setCharacterMaxWalkableAngle(RigidBodyHandle r, float maxWalkableAngle)
{
	PhysicsCharacter* c = getCharacter(r);

	if (c == nullptr) return;

	// Unsure if i should wake up the body here (i probably should cause if we change it and we're in a slope, the body will remain s
	c->maxWalkableAngle = maxWalkableAngle;
}

float PhysicsWorld::getCharacterStepHeight(RigidBodyHandle r)
{
	PhysicsCharacter* c = getCharacter(r);

	if (c == nullptr) return -1.0f;

	return c->stepHeight;
}

void PhysicsWorld::setCharacterStepHeight(RigidBodyHandle r, float stepHeight)
{
	PhysicsCharacter* c = getCharacter(r);

	if (c == nullptr) return;

	c->stepHeight = stepHeight;
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

	body.restitution = desc.restitution;
	body.friction = desc.friction;

	body.forceLayers = desc.forceLayers;
	body.bodyLayer = desc.bodyLayer;

	body.shapeHandle = desc.shape;

	body.isStatic = desc.isStatic;
	body.isKinematic = desc.isKinematic;
	body.isDynamic = desc.isDynamic;

	body.isCharacter = desc.isCharacter;
	body.isTrigger = desc.isTrigger;

	// vERY IMPORTANT
	body.bodyID = index;

	// USED but leave at default values
	body.sleeping = false;

	calculateAABB(&body);

	// Retain shape ref
	retainShape(desc.shape);

	bodies.Set(index, std::move(body));

	RigidBody* bodyPtr = bodies.GetPtr(index);

	// TODO: reminder to insert and remove into broadphase
	broadPhase.insert(bodies.GetPtr(index));

	// Extra: create character if body.isCharacter
	if (bodyPtr->isCharacter)
	{
		addCharacter(bodyPtr);
	}

	return { index };
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

	// Before deleting the body, delete the character info if it is a character
	if (body->isCharacter)
	{
		removeCharacter(handle);
	}

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

PhysicsCharacter* PhysicsWorld::getCharacter(RigidBodyHandle r)
{
	CharacterHandle cHandle = getCharacterHandle(r);

	if (!cHandle.isValid()) return nullptr;

	return characters.GetPtr(cHandle);
}

inline CharacterHandle PhysicsWorld::getCharacterHandle(RigidBodyHandle r)
{
	if (!r.isValid()) return InvalidCharacterHandle;

	auto it = bodyToCharacter.find(r);

	if (it == bodyToCharacter.end())
	{
		SDL_assert(false && "Invalid RigidBodyHandle to CharacterHandle");
		return InvalidCharacterHandle;
	}

	return it->second;
}

// We could call addCharacter using the handle, but im tired of using Get
// This is a private method anyway, and we can use body.bodyID for the handle index
void PhysicsWorld::addCharacter(RigidBody* body)
{
	if (body == nullptr) return;

	PhysicsCharacter defaultCharacter;
	defaultCharacter.body = body;

	// Check the index
	uint32_t index;
	if (freeCharacterIndices.size() == 0)
	{
		if (characters.Size() == UINT32_MAX)
		{
			SDL_assert(false && "RigidBody count has reached its limit, can't insert more");
		}

		index = characters.Size();
	}
	else
	{
		index = freeCharacterIndices.back();
		freeCharacterIndices.pop_back();
	}

	CharacterHandle cHandle = { index };

	characters.Set(index, std::move(defaultCharacter));
	bodyToCharacter.insert({ body->bodyID, cHandle });
}

void PhysicsWorld::removeCharacter(RigidBodyHandle r)
{
	if (!r.isValid()) return;

	CharacterHandle cHandle = getCharacterHandle(r);

	if (!cHandle.isValid()) return;

	characters.Delete(r.index);
	freeCharacterIndices.push_back(r.index);
	bodyToCharacter.erase(r);
}

// NOTE: to change the default character info please go into physics/core/types.h and change PhysicsCharacter's default values instead!!!!!!!!
void PhysicsWorld::setCharacterInfo(RigidBodyHandle r, const PhysicsCharacterInfo& info)
{
	PhysicsCharacter* c = getCharacter(r);

	// Comparing info. values to the default values, and setting them if they are not default ones
	if (info.groundState != PhysicsCharacter::GroundState::Undefined)
		c->groundState = info.groundState;
	if (info.groundNormal != glm::vec3(0.0f))
		c->groundNormal = info.groundNormal;
	if (info.maxWalkableAngle >= 0.0f)
		c->maxWalkableAngle = info.maxWalkableAngle;
	if (info.stepHeight >= 0.0f)
		c->stepHeight = info.stepHeight;
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
	// Wake all bodies up cause the new force gen should affect all bodies
	wakeAllBodies();
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
	{
		body->wakeUp();
		forceRegistry.addPair(body, gen);
	}
}

void PhysicsWorld::removeForce(RigidBodyHandle handle, IForceGenerator* gen)
{
	RigidBody* body = getBody(handle);
	if (body)
		forceRegistry.removePair(body, gen);
}


RaycastHit PhysicsWorld::raycast(const Ray& ray, const QueryFilter& filter) const
{
	std::vector<RaycastHit> hits = raycastAll(ray, filter);
	if (hits.empty()) return { /* Default (invalid) RaycastHit */ };

	size_t minIdx = 0;
	float minT = hits[0].t;
	for (size_t i = 1; i < hits.size(); i++)
	{
		if (hits[i].t < minT)
		{
			minIdx = i;
			minT = hits[i].t;
		}
	}

	return hits[minIdx];
}

std::vector<RaycastHit> PhysicsWorld::raycastAll(const Ray& ray, const QueryFilter& filter) const
{
	std::vector<RaycastHit> narrowHits;
	std::vector<RaycastHit> broadHits;
	broadPhase.queryRay(ray, getQueryFilterInternalFromQueryFilter(filter), broadHits);

	if (broadHits.empty()) return narrowHits;

	for (const RaycastHit& broadHit : broadHits)
	{
		// TODO: continue here
		// We might either skip narrowphase casting, orrr send the raycast information, or just double check using narrowphase
		RaycastHit hit = narrowPhase.raycast(ray, *broadHit.body, *this);
		if (hit.isValid())
		{
			hit.body = broadHit.body;
			narrowHits.push_back(hit);
		}
	}

	return narrowHits;
}

ShapecastHit PhysicsWorld::shapecast(const Ray& ray, ShapeHandle shape, const glm::quat& orientation, const QueryFilter& filter) const
{
	
	//RigidBody* target = nullptr;
	//if (optionalTargetBody.isValid())
	//{
	//	target = getBody(optionalTargetBody);
	//}
	//else
	//{
	//	RaycastHit rayHit = raycast(ray, filter);
	//	if (!rayHit.isValid()) return ShapecastHit::none();

	//	target = rayHit.body;
	//}
	//if (!target) return ShapecastHit::none();

	const IShape* queryShape = getShape(shape);
	if (!queryShape) return ShapecastHit::none();

	glm::vec3 shapePosition; glm::quat shapeOrientation;
	narrowPhase.resolveShapeTransform(queryShape, ray.origin, orientation, shapePosition, shapeOrientation);

	// ----
	// Broadphase of the calculated swept AABB from start to finish to get all candidates, then check 1 by 1
	// ---
	AABB aabbStart = queryShape->computeAABB(shapePosition, shapeOrientation);
	AABB aabbEnd = queryShape->computeAABB(shapePosition + ray.direction * ray.maxDistance, shapeOrientation);

	AABB sweptAABB = AABB::merge(aabbStart, aabbEnd);

	QueryFilterInternal filterInternal = getQueryFilterInternalFromQueryFilter(filter);

	std::vector<RigidBody*> candidates;
	broadPhase.queryAABB(sweptAABB, filterInternal, candidates);

	ShapecastHit closest{};

	for (RigidBody* body : candidates)
	{
		const IShape* targetShape = getShape(body->shapeHandle);

		glm::vec3 targetPosition; glm::quat targetOrientation;
		narrowPhase.resolveShapeTransform(targetShape, body->position, body->orientation, targetPosition, targetOrientation);
		
		ShapecastHit hit = narrowPhase.shapecast(ray, queryShape, shapePosition, shapeOrientation, targetShape, targetPosition, targetOrientation);

		if (!hit.isValid()) continue;

		hit.body = body; // important bit

		if (!closest.isValid() || hit.t < closest.t)
			closest = hit;
	}


	return closest;
}

std::vector<RigidBodyHandle> PhysicsWorld::shapeIntersects(ShapeHandle shapeHandle, const glm::vec3& position, const glm::quat& orientation, const QueryFilter& filter) const
{
	std::vector<RigidBody*> broadHits;
	std::vector<RigidBodyHandle> hits;

	const IShape* shape = getShape(shapeHandle);
	// TODO: keep going
	// Let's create an AABB out of the shape, check on broadphase and then
	glm::vec3 shapePosition;
	glm::quat shapeOrientation;
	narrowPhase.resolveShapeTransform(shape,
		position, orientation,
		shapePosition, shapeOrientation);

	AABB aabb = shape->computeAABB(shapePosition, shapeOrientation);

	broadPhase.queryAABB(aabb, getQueryFilterInternalFromQueryFilter(filter), broadHits);

	for (RigidBody* body : broadHits)
	{
		if (narrowPhase.testShapeIntersects(shape, shapePosition, shapeOrientation, *body, *this))
		{
			hits.push_back({ body->bodyID });
		}
	}

	return hits;
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
	applyForces(dt); // applies all global and paired forces
	integrate(dt); // integrates velocity and position
	updateBroadPhase(); // updates broadphase based on the new positions of the moving bodies
	detectCollisions(); // detects collisions at broadphase and narrowphase, fills contacts later resolved in solve()
	testPlanes(); // (not using planes) 
	
	solve(dt); // solves contacts (extra functionality for characters)
	updateSleep(dt); // updates the sleep on moving bodies

	dispatchEvents();
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
		if (body.sleeping || body.isStatic || body.isTrigger) continue;

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
		if (pair.bodyA->isKinematic && pair.bodyB->isKinematic) continue;

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
			if (body.sleeping || body.isStatic || body.isKinematic || body.isTrigger) continue;

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

void PhysicsWorld::updateSleep(float dt)
{
	for (RigidBody& body : bodies.Data())
	{
		if (body.sleeping || body.isStatic || body.isKinematic || body.isTrigger) continue;

		float kineticEnergy = 0.5f * body.mass * glm::dot(body.velocity, body.velocity); // velocity squared, we can just square the threshold
		float bias = 1.0f - std::pow(g_PhysicsSettings.sleepBiasRate, dt);

		body.accumulatedEnergy = (1.0f - bias) * body.accumulatedEnergy + bias * kineticEnergy;

		if (body.accumulatedEnergy < g_PhysicsSettings.energyThreshold)
			body.sleepTimer += dt;
		else
			body.sleepTimer = 0.0f;

		if (body.sleepTimer > g_PhysicsSettings.sleepTimeRequired)
		{
			body.sleeping = true;
			body.velocity = glm::vec3(0.0f);
		}
	}
}


void PhysicsWorld::dispatchEvents()
{
	std::set<BodyPair> currentCollisionPairs;
	std::set<BodyPair> currentTriggerPairs;

	// --- FILL CURRENT PAIRS ---
	for (const Contact& c : contacts)
	{
		bool trigger = c.bodyA->isTrigger || (c.bodyB && c.bodyB->isTrigger);

		BodyPair pair = BodyPair(c.bodyA, c.bodyB);
		RigidBodyHandle handleA = { c.bodyA->bodyID };
		RigidBodyHandle handleB;
		if (c.bodyB != nullptr)
			handleB = { c.bodyB->bodyID };

		if (trigger)
		{
			bool isStaying = previousTriggerPairs.count(pair) > 0;

			if (isStaying)
			{
				if (onTriggerStay) onTriggerStay(handleA, handleB, c);
			}
			else
			{
				if (onTriggerEnter) onTriggerEnter(handleA, handleB, c);
			}

			// Finally we add it to the current trigger pairs for the next step
			currentTriggerPairs.insert(pair);
		}
		else
		{
			bool isStaying = previousCollisionPairs.count(pair) > 0;

			if (isStaying)
			{
				if (onCollisionStay) onCollisionStay(handleA, handleB, c);
			}
			else
			{
				if (onCollisionEnter) onCollisionEnter(handleA, handleB, c);
			}

			// Finally we add it to the current collision pairs for the next step
			currentCollisionPairs.insert(pair);
		}
	}

	// --- COLLISION EXIT EVENT ---
	for (const BodyPair& pair : previousCollisionPairs)
	{
		if (currentCollisionPairs.count(pair) == 0)
		{
			RigidBodyHandle handleA = { pair.bodyA->bodyID };
			RigidBodyHandle handleB;
			if (pair.bodyB != nullptr)
				handleB = { pair.bodyB->bodyID };

			if (onCollisionExit) onCollisionExit(handleA, handleB);
		}
	}

	// --- TRIGGER EXIT EVENT ---
	for (const BodyPair& pair : previousTriggerPairs)
	{
		if (currentTriggerPairs.count(pair) == 0)
		{
			RigidBodyHandle handleA = { pair.bodyA->bodyID };
			RigidBodyHandle handleB;
			if (pair.bodyB != nullptr)
				handleB = { pair.bodyB->bodyID };

			if (onTriggerExit) onTriggerExit(handleA, handleB);
		}
	}

	// REPLACE CURRENT ONES
	previousCollisionPairs = currentCollisionPairs;
	previousTriggerPairs = currentTriggerPairs;
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

QueryFilterInternal PhysicsWorld::getQueryFilterInternalFromQueryFilter(const QueryFilter& queryFilter) const
{
	QueryFilterInternal res;

	if (queryFilter.bodyToIgnore != InvalidRigidBodyHandle)
		res.bodyToIgnore = getBody(queryFilter.bodyToIgnore);
	res.hasLayerOfQuery = queryFilter.hasLayerOfQuery;
	res.layerOfQuery = queryFilter.layerOfQuery;

	return res;
}

void PhysicsWorld::teleportBodyRaw(RigidBody* body, const glm::vec3& worldPosition)
{
	body->wakeUp();
	body->position = worldPosition; // not doing extra checks at the moment because i do NOT have time, but should probably check for collisions (if it's a valid place to teleport)
}

void PhysicsWorld::wakeAllBodies()
{
	for (RigidBody& body : bodies.Data())
	{
		body.wakeUp();
	}
}
