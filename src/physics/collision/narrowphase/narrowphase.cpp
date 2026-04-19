#include "narrowphase.h"

#include "physics/physics_world.h"

Contact NarrowPhase::testPair(const RigidBody& a, const RigidBody& b, const PhysicsWorld& world) const
{
	const IShape* shapeA = world.getShape(a.shapeHandle);
	const IShape* shapeB = world.getShape(b.shapeHandle);

	SDL_assert(shapeA && "Body A has null shape");
	SDL_assert(shapeB && "Body A has null shape");

	glm::vec3 positionA; glm::quat orientationA;
	glm::vec3 positionB; glm::quat orientationB;

	resolveShapeTransform(shapeA, a.position, a.orientation, positionA, orientationA);
	resolveShapeTransform(shapeB, b.position, b.orientation, positionB, orientationB);

	Contact c = dispatch(shapeA, positionA, orientationA, shapeB, positionB, orientationB);

	if (!c.isValid()) return Contact::none();

	// Add the rigidbodies because we don't pass them to dispatch
	// If we were to do analytic stuff and have the shapes changed, change the bodies back
	// to do so, uncomment the next bit and put the other in an else
	//if (swapped)
	//{
	//	c.bodyA = const_cast<RigidBody*>(&b);
	//	c.bodyB = const_cast<RigidBody*>(&a);
	//}

	c.bodyA = const_cast<RigidBody*>(&a);
	c.bodyB = const_cast<RigidBody*>(&b);

	return c;
}

Contact NarrowPhase::testPlane(const RigidBody& a, const PlaneShape& plane, const PhysicsWorld& world) const
{
	return Contact();
}

RaycastHit NarrowPhase::raycast(const Ray& ray, const RigidBody& body, const PhysicsWorld& world) const
{
	return RaycastHit();
}

RaycastHit NarrowPhase::raycastPlane(const Ray& ray, const PlaneShape& plane, const PhysicsWorld& world) const
{
	return RaycastHit();
}

void NarrowPhase::resolveShapeTransform(const IShape* shape,
	const glm::vec3& bodyPosition, const glm::quat& bodyOrientation, 
	glm::vec3& outPosition, glm::quat& outOrientation) const
{
	if (!shape->hasOffset())
	{
		outPosition = bodyPosition;
		outOrientation = bodyOrientation;
		return;
	}

	outPosition = bodyPosition + shape->localOffset;
	outOrientation = bodyOrientation * shape->localOrientation;

	return;
}

Contact NarrowPhase::dispatch(
	const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA, 
	const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB) const
{

	// For now we're just going to do GJK, except Sphere_Sphere.
	// TODO: IF we want more analytic results, we would just need to check the types and call the analytic results
	// For a better way of comparing shapes, just order the bodies in type, compare once and dispatch. 
	// We would need to check if we flipped the shape order and Invert the contact accordingly
	Contact c;
	bool analytic = false;

	// ANALYTIC PATHS (only Sphere-Sphere for now, the rest is GJK+EPA)
	if (shapeA->getType() == ShapeType::Sphere && shapeB->getType() == ShapeType::Sphere)
	{
		// we could just do return testSphere() instead of analytic = true;
		c = testSphereSphere(
			static_cast<const SphereShape&>(*shapeA), posA,
			static_cast<const SphereShape&>(*shapeB), posB);
		analytic = true;
	}


	// GJK + EPA (fallback after analytic paths)
	if (!analytic)
		c = testGJK_EPA(shapeA, posA, oriA, shapeB, posB, oriB);

	return c;
}

Contact NarrowPhase::testSphereSphere(const SphereShape& a, const glm::vec3& posA, const SphereShape& b, const glm::vec3& posB) const
{
	// We need to check for weird cases (spheres with same position) because we have triggers and those do not solve interpenetration
	glm::vec3 posBA = posA - posB;
	float dist = glm::length(posBA);
	float radiiSum = a.radius + b.radius;

	if (dist >= radiiSum) return Contact::none();

	glm::vec3 normal = (dist > F_EPSILON) ? posBA / dist : glm::vec3(0.0f, 1.0f, 0.0f);
	
	Contact c;
	c.normal = normal;
	c.depth = radiiSum - dist;
	c.pointA = posA + -normal * a.radius;
	c.pointA = posA + -normal * a.radius;
	c.pointB = posB + normal * b.radius;
	c.point = (posA + posB) * 0.5f;

	return c;
}

Contact NarrowPhase::testSpherePlane(const SphereShape& a, const glm::vec3& posA, const PlaneShape& plane) const
{
	return Contact();
}

Contact NarrowPhase::testBoxPlane(const BoxShape& box, const glm::vec3& pos, const glm::quat& ori, const PlaneShape& plane) const
{
	return Contact();
}

Contact NarrowPhase::testCapsulePlane(const CapsuleShape& a, const glm::vec3& posA, const glm::quat& ori, const PlaneShape& plane) const
{
	return Contact();
}

Contact NarrowPhase::testGJK_EPA(const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA, const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB) const
{
	GJKResult gjk = gjk_runGJK(shapeA, posA, oriA, shapeB, posB, oriB);

	if (!gjk.intersecting)
		return Contact::none();

	EPAResult epa = epa_runEPA(shapeA, posA, oriA, shapeB, posB, oriB, gjk);

	Contact c;
	c.point = epa.point;
	c.pointA = epa.pointA;
	c.pointB = epa.pointB;

	c.normal = epa.normal;
	c.depth = epa.depth;

	return c;
}

RaycastHit NarrowPhase::raycastSphere(const Ray& ray, const SphereShape& sphere, const glm::vec3& pos) const
{
	return RaycastHit();
}

RaycastHit NarrowPhase::raycastBox(const Ray& ray, const BoxShape& box, const glm::vec3& pos, const glm::quat& ori) const
{
	return RaycastHit();
}

RaycastHit NarrowPhase::raycastCapsule(const Ray& ray, const CapsuleShape& capsule, const glm::vec3& pos, const glm::quat& ori) const
{
	return RaycastHit();
}
