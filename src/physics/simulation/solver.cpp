#include "solver.h"

void Solver::solve(const std::vector<Contact>& contacts, float dt)
{
	for (const Contact& contact : contacts)
	{
		// For now
		resolveInterpenetration(contact, dt);
	}
}

inline void Solver::resolveInterpenetration(const Contact& contact, float dt)
{
    RigidBody* a = contact.bodyA;
    RigidBody* b = contact.bodyB;   // may be null for plane contacts

    // if any of the bodies are triggers we skip resolving interpenetration
    if (a->isTrigger || b->isTrigger) return;

    // Total inverse mass
    float invMassA = a->invMass;
    float invMassB = b ? b->invMass : 0.0f;
    float totalInvMass = invMassA + invMassB;

    // will happen if both objects are static or kinematic
    // which shouldn't happen because we're not colliding static vs static or kinematic vs kinematic, idk
    if (totalInvMass <= 0.0f) return;

    glm::vec3 correction = contact.normal * contact.depth / totalInvMass;

    // Push bodyA out proportional to its inverse mass
    if (!a->isStatic && !a->isKinematic)
        a->position += correction * invMassA;

    // Push bodyB in opposite direction
    if (b && !b->isStatic && !b->isKinematic)
        b->position -= correction * invMassB;


    glm::vec3 relativeVelocity = glm::vec3(0.0f);

    relativeVelocity += a->velocity;
    if (b) 
        relativeVelocity -= b->velocity;

    float velAlongNormal = glm::dot(relativeVelocity, contact.normal);

    // if we are colliding but going away from each other
    if (velAlongNormal >= 0.0f) return;

    // Same as position correction
    glm::vec3 velocityCorrection = contact.normal * velAlongNormal / totalInvMass;

    if (!a->isStatic && !a->isKinematic)
        a->velocity -= velocityCorrection * invMassA;

    if (b && !b->isStatic && !b->isKinematic)
        b->velocity += velocityCorrection * invMassB;

}


