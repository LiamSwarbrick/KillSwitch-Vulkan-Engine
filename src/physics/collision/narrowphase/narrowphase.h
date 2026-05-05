#ifndef PHYSICS_COLLISION_NARROWPHASE_NARROWPHASE_H
#define PHYSICS_COLLISION_NARROWPHASE_NARROWPHASE_H

#include "physics/core/types.h"
#include "physics/collision/shapes/shape.h"
#include "physics/collision/shapes/sphere.h"
#include "physics/collision/shapes/box.h"
#include "physics/collision/shapes/capsule.h"
#include "physics/collision/shapes/plane.h"
#include "contact.h"

#include "physics/queries/raycast.h"

#include "gjk.h"
#include "epa.h"

// Forward declaring
class PhysicsWorld;

class NarrowPhase
{
public:
	// Main methods (everything is const cause its readonly
	Contact testPair(const RigidBody& a, const RigidBody& b, const PhysicsWorld& world) const;

	Contact testPlane(const RigidBody& a, const PlaneShape& plane, const PhysicsWorld& world) const;

	// Queries (for later)
	RaycastHit raycast(const Ray& ray, const RigidBody& body, const PhysicsWorld& world) const;
	
	bool testShapeIntersects(const IShape* shape, const glm::vec3& shapePosition, const glm::quat& shapeOrientation, const RigidBody& body, const PhysicsWorld& world) const;
	
	ShapecastHit shapecast(
		const Ray& ray, 
		const IShape* queryShape, const glm::vec3& queryPos, const glm::quat& queryOri,
		const IShape* targetShape, const glm::vec3& targetPos, const glm::quat& targetOri) const;

	RaycastHit raycastPlane(const Ray& ray, const PlaneShape& plane, const PhysicsWorld& world) const;

	// Helper to transform shapes's pos and ori in case the shape has offset
	void resolveShapeTransform(const IShape* shape,
		const glm::vec3& bodyPosition, const glm::quat& bodyOrientation,
		glm::vec3& outPosition, glm::quat& outOrientation) const;

private:
	Contact dispatch(
		const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA,
		const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB) const;

	// Analytic paths
	Contact testSphereSphere(
		const SphereShape& a, const glm::vec3& posA,
		const SphereShape& b, const glm::vec3& posB) const;

	Contact testSpherePlane(
		const SphereShape& a, const glm::vec3& posA,
		const PlaneShape& plane) const;

	Contact testBoxPlane(
		const BoxShape& box, const glm::vec3& pos, const glm::quat& ori,
		const PlaneShape& plane) const;

	Contact testCapsulePlane(
		const CapsuleShape& a, const glm::vec3& posA, const glm::quat& ori,
		const PlaneShape& plane) const;

	Contact testGJK_EPA(
		const IShape* shapeA, const glm::vec3& posA, const glm::quat& oriA,
		const IShape* shapeB, const glm::vec3& posB, const glm::quat& oriB) const;


	// For queries (in the future but defined already
	// Analytic ray vs shape test
	RaycastHit raycastSphere(
		const Ray& ray, 
		const SphereShape& sphere, const glm::vec3& pos) const;

	RaycastHit raycastBox(
		const Ray& ray,
		const BoxShape& box, const glm::vec3& pos, const glm::quat& ori) const;

	RaycastHit raycastCapsule(
		const Ray& ray,
		const CapsuleShape& capsule, const glm::vec3& pos, const glm::quat& ori) const;

	// Conservative Advancement for raycasting
	ShapecastHit conservativeAdvancement(
		const Ray& ray,
		const IShape* queryShape, const glm::vec3& queryPos, const glm::quat& queryOri,
		const IShape* targetShape, const glm::vec3& targetPos, const glm::quat& targetOri) const;
};

#endif // !PHYSICS_COLLISION_NARROWPHASE_NARROWPHASE_H