#include "solver.h"

#include "physics/physics_world.h"

struct Solver::ExtendedContact
{
    glm::vec3 point; // in world-space
    glm::vec3 normal; // from B to A

    // Extras just to debug
    glm::vec3 pointA;
    glm::vec3 pointB;

    float depth = 0.0f;

    RigidBody* bodyA = nullptr;
    PhysicsCharacter* charA = nullptr;
    bool isWalkableA = false;

    RigidBody* bodyB = nullptr;
    PhysicsCharacter* charB = nullptr;
    bool isWalkableB = false;

    float totalInvMass = 1.0f;

    ExtendedContact(const Contact& contact, PhysicsWorld& world)
    {
        point = contact.point;
        normal = contact.normal;
        pointA = contact.pointA;
        pointB = contact.pointB;

        depth = contact.depth;

        bodyA = contact.bodyA;
        bodyB = contact.bodyB;

        totalInvMass = contact.bodyA->invMass + (contact.bodyB ? contact.bodyB->invMass : 0.0f);

        if (bodyA->isCharacter)
        {
            charA = world.getCharacter({ bodyA->bodyID });

            // groundNormal instead of (0.0f, 1.0f, 0.0f)
            float upAlongNormal = glm::dot(normal, charA->groundNormal);
            float slopeAngle = glm::acos(upAlongNormal);

            isWalkableA = slopeAngle <= charA->maxWalkableAngle;

            // if the slope is walkable, correct only on the character's groundNormal axis
            if (isWalkableA)
            {
                normal = charA->groundNormal;
                charA->groundState = PhysicsCharacter::GroundState::OnGround;
            }
        }

        if (bodyB && bodyB->isCharacter)
        {
            charB = world.getCharacter({ bodyB->bodyID });
            // -contact.normal because normal goes from B to A
            // groundNormal instead of (0.0f, 1.0f, 0.0f)
            float upAlongNormal = glm::dot(-normal, charB->groundNormal);

            float slopeAngle = glm::acos(upAlongNormal);
            isWalkableB = slopeAngle <= charB->maxWalkableAngle;

            // if the slope is walkable, correct only on the character's groundNormal axis
            if (isWalkableB)
            {
                normal = (-charB->groundNormal);
                charB->groundState = PhysicsCharacter::GroundState::OnGround;
            }
        }
    }

};

void Solver::solve(const std::vector<Contact>& contacts, float dt)
{
    world.resetAllCharactersGroundState();

	for (const Contact& contact : contacts)
	{
        RigidBody* a = contact.bodyA;
        RigidBody* b = contact.bodyB;   // may be null for plane contacts (we do NOT have planes at all at the current version)

        // if any of the bodies are triggers we skip resolving interpenetration
        if (a->isTrigger || (b && b->isTrigger)) continue;

        // Before resolving interpenetration, velocities or extra character steps, 
        // we need to see if there is a character involved in the contact, and if the contact
        // is "walkable" for the character, meaning we would need to change the normal and depth
        ExtendedContact extendedContact(contact, world);

        // Resolve interpenetration and velocities (then try step up/down)
		resolveInterpenetration(extendedContact, dt);

        resolveVelocities(extendedContact, dt);
	}
}

bool Solver::changeContactDependingOnCharacterWalkability(ExtendedContact& contact, float dt)
{
    RigidBody* a = contact.bodyA;
    RigidBody* b = contact.bodyB;

    bool isWalkable = false;
    if (a->isCharacter)
    {
        PhysicsCharacter* character = world.getCharacter({ a->bodyID });
        contact.charA = character;

        // groundNormal instead of (0.0f, 1.0f, 0.0f)
        float upAlongNormal = glm::dot(contact.normal, character->groundNormal);
        float slopeAngle = glm::acos(upAlongNormal);

        isWalkable = slopeAngle <= character->maxWalkableAngle;

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (isWalkable)
        {
            contact.isWalkableA = true;
            contact.normal = character->groundNormal;
            character->groundState = PhysicsCharacter::GroundState::OnGround;
        }
    }

    if (!isWalkable && b && b->isCharacter)
    {
        PhysicsCharacter* character = world.getCharacter({ b->bodyID });
        // -contact.normal because normal goes from B to A
        // groundNormal instead of (0.0f, 1.0f, 0.0f)
        float upAlongNormal = glm::dot(-contact.normal, character->groundNormal);

        float slopeAngle = glm::acos(upAlongNormal);
        isWalkable = slopeAngle <= character->maxWalkableAngle;

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (isWalkable)
        {
            contact.isWalkableB = true;
            contact.normal = (-character->groundNormal);
            character->groundState = PhysicsCharacter::GroundState::OnGround;
        }
    }

    return isWalkable;
}

inline void Solver::resolveInterpenetration(const ExtendedContact& contact, float dt)
{
    RigidBody* a = contact.bodyA;
    RigidBody* b = contact.bodyB;
    // will happen if both objects are static or kinematic
    // which shouldn't happen because we're not colliding static vs static or kinematic vs kinematic, idk
    if (contact.totalInvMass <= 0.0f) return;


    glm::vec3 correction = contact.normal * contact.depth / contact.totalInvMass;

    // Push bodyA out proportional to its inverse mass
    if (!a->isStatic && !a->isKinematic)
    {
        if (contact.isWalkableA)
        {
            float upAlongNormal = glm::dot(contact.normal, contact.charA->groundNormal);
            a->position += correction / upAlongNormal * a->invMass;
        }
        else
            a->position += correction * a->invMass;
    }
        

    if (b && !b->isStatic && !b->isKinematic)
    {
        if (contact.isWalkableB)
        {
            float upAlongNormal = glm::dot(-contact.normal, contact.charB->groundNormal);
            b->position -= correction / upAlongNormal * b->invMass;
        }
        else
            b->position -= correction * b->invMass;
    }
        
}

void Solver::resolveVelocities(const ExtendedContact& contact, float dt)
{
    RigidBody* a = contact.bodyA;
    RigidBody* b = contact.bodyB;

    glm::vec3 relativeVelocity = a->velocity;
    if (b) relativeVelocity -= b->velocity;

    float velAlongNormal = glm::dot(relativeVelocity, contact.normal);

    // if we are colliding but going away from each other
    if (velAlongNormal >= 0.0f) return;

    //TODO: keep going for full impulse resolution

    // Same as position correction
    glm::vec3 velCorrectionAlongNormal = contact.normal * velAlongNormal / contact.totalInvMass;
    float restitution = a->restitution * (b ? b->restitution : 0.0f);

    float j = -(1.0f + restitution) * velAlongNormal / contact.totalInvMass;
    glm::vec3 impulse = contact.normal * j;

    if (!a->isStatic && !a->isKinematic)
    {
        if (a->isCharacter)
            a->velocity -= velCorrectionAlongNormal * a->invMass;
        else
            a->velocity += impulse * a->invMass;
    }

    if (b && !b->isStatic && !b->isKinematic)
    {
        if (b->isCharacter)
        {
            b->velocity += velCorrectionAlongNormal * b->invMass;
        }
        else
            b->velocity -= impulse * b->invMass;
    }

    // Friction (no angular momentum)
    relativeVelocity = a->velocity;
    if (b) relativeVelocity -= b->velocity;
    glm::vec3 velCorrectionAlongTangent = relativeVelocity - contact.normal * velAlongNormal;
    glm::vec3 tangentDir = glm::normalize(velCorrectionAlongTangent);
    float frictionCoeff = a->friction * (b ? b->friction : 1.0f);

    float jt = glm::clamp(
        -glm::dot(relativeVelocity, tangentDir) / contact.totalInvMass,
        -frictionCoeff * j, frictionCoeff * j
    );

    glm::vec3 frictionImpulse = tangentDir * jt;

    if (!a->isStatic && !a->isKinematic)
    {
        if(!a->isCharacter)
            a->velocity += frictionImpulse * a->invMass;
    }

    if (b && !b->isStatic && !b->isKinematic)
    {
        if(!b->isCharacter)
            b->velocity -= frictionImpulse * b->invMass;
    }
    
}


