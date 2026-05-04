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
};

void Solver::solve(const std::vector<Contact>& contacts, float dt)
{
    // Before solving contacts, prepare characters for extra logic
    for (PhysicsCharacter& c : world.characters.Data())
    {
        RigidBody* b = c.body;
        c.groundState = PhysicsCharacter::GroundState::InAir;

        // Store the position and velocity before resolving contacts of players, that way we can work out step-ups
        c.prePosition = b->position;
        c.preVelocity = b->velocity;
    }

	for (const Contact& contact : contacts)
	{
        RigidBody* a = contact.bodyA;
        RigidBody* b = contact.bodyB;   // may be null for plane contacts (we do NOT have planes at all at the current version)

        // if any of the bodies are triggers we skip resolving interpenetration
        if (a->isTrigger || (b && b->isTrigger)) continue;

        // Before resolving interpenetration, velocities or extra character steps, 
        // we need to see if there is a character involved in the contact, and if the contact
        // is "walkable" for the character, meaning we would need to change the normal and depth
        ExtendedContact extendedContact;
        FillExtendedContact(contact, extendedContact, dt);

        // Resolve interpenetration and velocities (then try step up/down)
		resolveInterpenetration(extendedContact, dt);

        resolveVelocities(extendedContact, dt);
	}

    for (PhysicsCharacter& c : world.characters.Data())
    {
        RigidBody* b = c.body;
        //tryStepUp(c, *b, dt);
        tryStepDown(c, *b, dt);
    }

}

void Solver::FillExtendedContact(const Contact& contact, ExtendedContact& outExtContact, float dt)
{
    outExtContact.point = contact.point;
    outExtContact.normal = contact.normal;
    outExtContact.pointA = contact.pointA;
    outExtContact.pointB = contact.pointB;

    outExtContact.depth = contact.depth;

    outExtContact.bodyA = contact.bodyA;
    outExtContact.bodyB = contact.bodyB;

    outExtContact.totalInvMass = contact.bodyA->invMass + (contact.bodyB ? contact.bodyB->invMass : 0.0f);

    if (contact.bodyA->isCharacter)
    {
        outExtContact.charA = world.getCharacter({ contact.bodyA->bodyID });

        // 
        float upAlongNormal = glm::dot(contact.normal, world.UP_VECTOR);
        float slopeAngle = glm::acos(upAlongNormal);

        outExtContact.isWalkableA = slopeAngle <= outExtContact.charA->maxWalkableAngle;

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (outExtContact.isWalkableA)
        {
            outExtContact.charA->groundState = PhysicsCharacter::GroundState::OnGround;
            outExtContact.charA->jumping = false;

            outExtContact.charA->groundNormal = contact.normal; // Changing to contact.normal so the player can move properly game-side
            // setting the contact.normal to be UP_VECTOR so the contact resolution allows the player to stand on edges and other rigidbodies without pushing them horizontally (unless slope too much)
            outExtContact.normal = world.UP_VECTOR;
        }
        else if (outExtContact.charA->groundState != PhysicsCharacter::GroundState::OnGround)
        {
            // If it is not walkable, AND the character is NOT on the ground
            // We need to check if we are at a slope or we're hitting a wall / ceiling

            // If upAlongNormal is close or less than 0, we are InAir,
            // If it is bigger than 0, we are in a slope

            // We should probably change F_EPSILON to a custom value, to detect walls properly maybe
            if (upAlongNormal > WALL_NORMAL_Y)
                outExtContact.charA->groundState = PhysicsCharacter::GroundState::OnSteepGround;
        }

    }

    if (outExtContact.bodyB && outExtContact.bodyB->isCharacter)
    {
        outExtContact.charB = world.getCharacter({ outExtContact.bodyB->bodyID });
        // -contact.normal because normal goes from B to A
        // groundNormal instead of (0.0f, 1.0f, 0.0f)
        float upAlongNormal = glm::dot(-contact.normal, world.UP_VECTOR);

        float slopeAngle = glm::acos(upAlongNormal);
        outExtContact.isWalkableB = slopeAngle <= outExtContact.charB->maxWalkableAngle;

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (outExtContact.isWalkableB)
        {
            outExtContact.charB->groundState = PhysicsCharacter::GroundState::OnGround;
            outExtContact.charB->jumping = false;

            outExtContact.charB->groundNormal = -contact.normal; // Changing to contact.normal so the player can move properly game-side
            // setting the contact.normal to be -UP_VECTOR so the contact resolution allows the player to stand on edges and other rigidbodies without pushing them horizontally (unless slope too much)
            outExtContact.normal = -world.UP_VECTOR;
        }
        else if (outExtContact.charB->groundState != PhysicsCharacter::GroundState::OnGround)
        {
            // If it is not walkable, AND the character is NOT on the ground
            // We need to check if we are at a slope or we're hitting a wall / ceiling

            // If upAlongNormal is close or less than 0, we are InAir,
            // If it is bigger than 0, we are in a slope

            // We should probably change F_EPSILON to a custom value, to detect walls properly maybe
            if (upAlongNormal > WALL_NORMAL_Y)
                outExtContact.charB->groundState = PhysicsCharacter::GroundState::OnSteepGround;
        }
    }

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

void Solver::resolveCharacter(PhysicsCharacter& character, RigidBody& body, float dt)
{
}

bool Solver::tryStepUp(PhysicsCharacter& character, RigidBody& body, float dt)
{

    return true;
}

bool Solver::tryStepDown(PhysicsCharacter& character, RigidBody& body, float dt)
{
    if (character.groundState == PhysicsCharacter::GroundState::InAir && !character.jumping)
    {
        RaycastHit hit;
        QueryFilter filter;
        filter.bodyToIgnore = { body.bodyID };

        IShape* ishape = world.getShape(body.shapeHandle);
        CapsuleShape* capsule = static_cast<CapsuleShape*>(ishape);
        float characterHeight = capsule->halfHeight + capsule->radius;
        
        Ray ray;

        ray.origin = body.position;
        ray.origin.y -= characterHeight;
        ray.direction = -world.UP_VECTOR; // Might have to change for -character.groundNormal (we will see)
        ray.maxDistance = character.stepHeight;

        // CURRENTLY DOESN'T WORK WITH RAYCAST, IMPLEMENT RAYCAST AND SWITCH TO THAT
        // Basically it jitters because it doesn't take into account the shape, just ray
        hit = world.raycast(ray, filter);
        if (hit.isValid())
        {
            float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
            float slopeAngle = glm::acos(upAlongNormal);

            if (slopeAngle <= character.maxWalkableAngle)
            {
                body.position = body.position - world.UP_VECTOR * hit.t;
                body.velocity.y = 0.0f; // Testing
                character.groundNormal = hit.normal;
                character.groundState = PhysicsCharacter::GroundState::OnGround;
                character.jumping = false;
            }
            else
            {
                body.position = hit.point + characterHeight;
                body.velocity.y = 0.0f; // Testing
                character.groundNormal = world.UP_VECTOR;
                character.groundState = PhysicsCharacter::GroundState::OnSteepGround;
            }
        }
    }

    return false;
}




