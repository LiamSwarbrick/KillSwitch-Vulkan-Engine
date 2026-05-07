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
    prepareCharacters(dt);

	for (const Contact& contact : contacts)
	{
        RigidBody* a = contact.bodyA;
        RigidBody* b = contact.bodyB;   // may be null for plane contacts (we do NOT have planes at all at the current version)

        if (a->sleeping) a->wakeUp();
        if (b && b->sleeping) b->wakeUp();
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

    resolveCharactersExtended(dt);
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

        if (!outExtContact.isWalkableA)
        {
            outExtContact.charA->lastNonWalkableNormalContact = contact.normal;
            // We also need to reset our baseVelocity (do not reset if we jump, we want to keep the momentum of the kinematic object if we jump)
            outExtContact.charA->baseVelocity = glm::vec3(0.0f);
        }

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (outExtContact.isWalkableA)
        {
            outExtContact.charA->groundState = PhysicsCharacter::GroundState::OnGround;
            outExtContact.charA->jumping = false;

            outExtContact.charA->groundNormal = contact.normal; // Changing to contact.normal so the player can move properly game-side
            // setting the contact.normal to be UP_VECTOR so the contact resolution allows the player to stand on edges and other rigidbodies without pushing them horizontally (unless slope too much)
            outExtContact.normal = world.UP_VECTOR;

            // Check if the other walkable contact is kinematic and adhere to it 
            if (contact.bodyB && contact.bodyB->isKinematic)
                outExtContact.charA->baseVelocity= contact.bodyB->velocity;
            else
                outExtContact.charA->baseVelocity = glm::vec3(0.0f);
        }
        else if (outExtContact.charA->groundState != PhysicsCharacter::GroundState::OnGround)
        {
            // If it is not walkable, AND the character is NOT on the ground
            // We need to check if we are at a slope or we're hitting a wall / ceiling

            // If upAlongNormal is close or less than 0, we are InAir,
            // If it is bigger than 0, we are in a slope
            outExtContact.charA->groundNormal = world.UP_VECTOR;
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

        if (!outExtContact.isWalkableB)
        {
            outExtContact.charB->lastNonWalkableNormalContact = -contact.normal;
            // We also need to reset our baseVelocity (do not reset if we jump, we want to keep the momentum of the kinematic object if we jump)
            // Only reset on a non-walkable contact 
            outExtContact.charB->baseVelocity = glm::vec3(0.0f);
        }

        // if the slope is walkable, correct only on the character's groundNormal axis
        if (outExtContact.isWalkableB)
        {
            outExtContact.charB->groundState = PhysicsCharacter::GroundState::OnGround;
            outExtContact.charB->jumping = false;

            outExtContact.charB->groundNormal = -contact.normal; // Changing to contact.normal so the player can move properly game-side
            // setting the contact.normal to be -UP_VECTOR so the contact resolution allows the player to stand on edges and other rigidbodies without pushing them horizontally (unless slope too much)
            outExtContact.normal = -world.UP_VECTOR;

            // Check if the other walkable contact is kinematic and adhere to it 
            if (contact.bodyA->isKinematic)
                outExtContact.charB->baseVelocity = contact.bodyA->velocity;
            else
                outExtContact.charB->baseVelocity = glm::vec3(0.0f);
        }
        else if (outExtContact.charB->groundState != PhysicsCharacter::GroundState::OnGround)
        {
            // If it is not walkable, AND the character is NOT on the ground
            // We need to check if we are at a slope or we're hitting a wall / ceiling

            // If upAlongNormal is close or less than 0, we are InAir,
            // If it is bigger than 0, we are in a slope
            outExtContact.charB->groundNormal = world.UP_VECTOR;
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
    // redundant bc if static or kinematic, invMass = 0 so the position wouldn't change
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

void Solver::prepareCharacters(float dt)
{
    for (PhysicsCharacter& c : world.characters.Data())
    {
        RigidBody* b = c.body;
        if (b->sleeping) continue;
        c.lastFrameGroundState = c.groundState;
        c.groundNormal = world.UP_VECTOR;

        // Store the position and velocity before resolving contacts of players, that way we can work out step-ups
        c.preSolvingPosition = b->position;
        c.preSolvingVelocity = c.getRelativeVelocity();
        c.lastNonWalkableNormalContact = glm::vec3(0.0f); // storing 0,0,0

        c.groundState = PhysicsCharacter::GroundState::InAir;
    }
}

void Solver::resolveCharactersExtended(float dt)
{
    // Resolving the character's position
    for (PhysicsCharacter& c : world.characters.Data())
    {
        RigidBody* b = c.body;
        if (b->sleeping) continue;

        // glm::vec3 desiredHorizontalVelocity = glm::vec3(c.preSolvingVelocity.x, 0.0f, c.preSolvingVelocity.z);
        // float currentForwardLength = glm::length(desiredHorizontalVelocity) * dt;
        // const float LENGTH_TOLERANCE = 0.01f;
        // if (currentForwardLength <= LENGTH_TOLERANCE) continue;

        if(!tryStepUp(c, *b, dt))
            trySnapDown(c, *b, dt); // Only snap down if we have not stepped up, because stepping up makes us snap down to the step.

        // IMPORTANT: this is not going to work, assuming we only run 1 step per in-game character update, the character is just going to override the velocity
        // First fix that comes to mind is: to, for characters, use setCharacterVelocity, so it sets a character::inputVelocity instead, that way we will preserve last frame's speed? idk.
        // TODO: To take into account later when i test kinematic objects properly
    }
}

bool Solver::tryStepUp(PhysicsCharacter& character, RigidBody& body, float dt)
{
    if (character.groundState != PhysicsCharacter::GroundState::OnGround) return false;

    // If we were on Ground contact, let's check if last blocking normal is not 0,0,0 (meaning we were blocked)
    if (character.lastNonWalkableNormalContact == glm::vec3(0.0f)) return false;

    glm::vec3 desiredHorizontalVelocity = glm::vec3(character.preSolvingVelocity.x, 0.0f, character.preSolvingVelocity.z);
    float currentForwardLength = glm::length(desiredHorizontalVelocity) * dt;
    const float LENGTH_TOLERANCE = 0.01f;
    if (currentForwardLength <= LENGTH_TOLERANCE) return false;
    
    // Here now we can try to step-up
    bool steppedUp = false;

    // Before shapecasting our way up, we need to precalculate some positions
    // we have to back-track the position before the integration step
    glm::vec3 preIntegratedPosition = character.preSolvingPosition - character.preSolvingVelocity * dt;
    
    glm::vec3 desiredDirection = glm::normalize(desiredHorizontalVelocity);
    
    glm::vec3 desiredPosition = character.preSolvingPosition;

    // Shape of the character
    ShapeHandle shape = body.shapeHandle;

    
    QueryFilter filter = {
        .bodyToIgnore = { body.bodyID }
        // We could use a filter (even though they are defined at game-side)
        // I might change the definition of game-side to engine-side but a separate class
    }; 
    glm::quat orientation = glm::identity<glm::quat>();

    ShapecastHit hit;

    // Quick fix for small steps, instead of velocity*dt, we can try character radius to never be unable to climb stairs
    IShape* shapePtr = world.getShape(body.shapeHandle);
    CapsuleShape* capsule = static_cast<CapsuleShape*>(shapePtr);
    //float desiredForwardLength = std::max(currentForwardLength, capsule->radius);
    float desiredForwardLength = currentForwardLength;
    
    // MORE ELABORATE FIX: shapecast to the front, get hit.pointA,
    // get pointA to center of object, thats the effective distance we have to move forward to go up the stair (we could also check with the hit.normal to increase distance and fully go up the stair)
    // 
    // distance detect ray
    Ray ddRay = {
        .origin = preIntegratedPosition,
        .direction = desiredDirection,
        .maxDistance = currentForwardLength + 0.01f,
    };
    hit = world.shapecast(ddRay, shape, orientation, filter);
    if (hit.isValid())
    {
        glm::vec3 projectedPosition = ddRay.origin + ddRay.direction * ddRay.maxDistance * hit.t;
        glm::vec3 projectedDifferenceToGo = projectedPosition - hit.pointA;
        projectedDifferenceToGo.y = 0.0f;
        float projectedDistanceToGo = glm::length(projectedDifferenceToGo) + 0.01f; // smol padding

        desiredForwardLength = std::max(desiredForwardLength, projectedDistanceToGo);
    }
    else
    {
        SDL_assert(false && "We should be colliding wtf :sob:");
        desiredForwardLength = std::max(desiredForwardLength, capsule->radius);
    }

    // shapecast up to check if we have room.
    Ray upRay = {
        .origin = preIntegratedPosition,
        .direction = world.UP_VECTOR,
        .maxDistance = character.stepHeight
    };

    hit = world.shapecast(upRay, shape, orientation, filter);
    // if there is no hit we can move fully, otherwise we could try moving UP to the hit.
    float distanceTravelled = 1.0f;
    if (hit.isValid())
        distanceTravelled = hit.t;



    // shapecast in the desired direction
    Ray forwardRay = {
        .origin = upRay.origin + upRay.direction * upRay.maxDistance * distanceTravelled,
        .direction = desiredDirection,
        .maxDistance = desiredForwardLength
    };

    hit = world.shapecast(forwardRay, shape, orientation, filter);
    distanceTravelled = 1.0f;

    bool extraShapecastDiagonally = false;
    // If there is a hit, check the normal of the contact, and if it is NOT walkable then return false
    if (hit.isValid())
    {
        distanceTravelled = hit.t;

        float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
        float slopeAngle = glm::acos(upAlongNormal);
        bool isWalkable = slopeAngle <= character.maxWalkableAngle;

        if (isWalkable)
        {
            // If it is walkable, we teleport there and then skip the snap down
            body.position = forwardRay.origin + forwardRay.direction * forwardRay.maxDistance * hit.t;
            body.velocity.y = 0.0f; // Testing
            character.groundNormal = hit.normal;
            character.groundState = PhysicsCharacter::GroundState::OnGround;
            character.jumping = false;

            if (hit.body->isKinematic)
                character.baseVelocity = hit.body->velocity;
            else
                character.baseVelocity = glm::vec3(0.0f);

            return true;
        }
        else
        {
            // we could have hit some ceiling or a next step which would make us not keep going.
            // in that case do an extra shapecast, and if that fails then we should NOT move

            // we could also take into account the distance travelled, and simply substract a bit to the distanceTravelled and try snapping down from there.
            // will try a diagonal shapecast for now
            extraShapecastDiagonally = true;
        }
    }

    // extra shapecast diagonally, changing forward ray's direction and maxDistance
    if (extraShapecastDiagonally)
    {
        glm::vec3 newPos = forwardRay.direction * forwardRay.maxDistance - (world.UP_VECTOR * character.stepHeight);
        forwardRay.direction = glm::normalize(newPos);
        forwardRay.maxDistance = glm::length(newPos);

        hit = world.shapecast(forwardRay, shape, orientation, filter);
        if (hit.isValid())
        {
            distanceTravelled = hit.t;

            float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
            float slopeAngle = glm::acos(upAlongNormal);
            bool isWalkable = slopeAngle <= character.maxWalkableAngle;

            if (isWalkable)
            {
                // If it is walkable, we teleport there and then skip the snap down
                body.position = forwardRay.origin + forwardRay.direction * forwardRay.maxDistance * hit.t;
                body.velocity.y = 0.0f; // Testing
                character.groundNormal = hit.normal;
                character.groundState = PhysicsCharacter::GroundState::OnGround;
                character.jumping = false;

                if (hit.body->isKinematic)
                    character.baseVelocity = hit.body->velocity;
                else
                    character.baseVelocity = glm::vec3(0.0f);

                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            // Very weird case (we should have hit at least the floor)
            return false;
        }
    }

    // snap down again IFF we are able to go MAX DISTANCE FORWARD
    Ray downRay = {
        .origin = forwardRay.origin + forwardRay.direction * forwardRay.maxDistance * distanceTravelled,
        .direction = -world.UP_VECTOR,
        .maxDistance = character.stepHeight
    };
    hit = world.shapecast(downRay, shape, orientation, filter);
    if (hit.isValid())
    {
        distanceTravelled = hit.t;

        float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
        float slopeAngle = glm::acos(upAlongNormal);
        bool isWalkable = slopeAngle <= character.maxWalkableAngle;

        if (isWalkable)
        {
            // If it is walkable, we teleport there and then skip the snap down
            body.position = downRay.origin + downRay.direction * downRay.maxDistance * hit.t;
            body.velocity.y = 0.0f; // Testing
            character.groundNormal = hit.normal;
            character.groundState = PhysicsCharacter::GroundState::OnGround;
            character.jumping = false;

            if (hit.body->isKinematic)
                character.baseVelocity = hit.body->velocity;
            else
                character.baseVelocity = glm::vec3(0.0f);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        // We should always hit the floor, return false
        return false;
    }

    return false;
}



bool Solver::trySnapDown(PhysicsCharacter& character, RigidBody& body, float dt)
{
    if (character.lastFrameGroundState == PhysicsCharacter::InAir ||
        character.groundState != PhysicsCharacter::GroundState::InAir ||
        character.jumping) 
        return false;

    
    bool snappedDown = false;
    
    QueryFilter filter;
    filter.bodyToIgnore = { body.bodyID };
    glm::quat orientation = glm::identity<glm::quat>();

    IShape* ishape = world.getShape(body.shapeHandle);
    CapsuleShape* capsule = static_cast<CapsuleShape*>(ishape);
    float characterHeight = capsule->halfHeight + capsule->radius;

    glm::vec3 desiredHorizontalVelocity = glm::vec3(character.preSolvingVelocity.x, 0.0f, character.preSolvingVelocity.z);
    glm::vec3 desiredHorizontalDir = glm::normalize(desiredHorizontalVelocity);
    float currentForwardLength = glm::length(desiredHorizontalVelocity) * dt;
    //float desiredForwardLength = std::max(currentForwardLength, capsule->radius);
    const float LENGTH_TOLERANCE = 0.01f;
    if (currentForwardLength <= LENGTH_TOLERANCE) return false;
    float desiredForwardLength = currentForwardLength;

    Ray snapDownRay = {
        .origin = body.position,
        .direction = -world.UP_VECTOR, // might change it for -character.groundNormal
        .maxDistance = character.stepHeight
    };

    bool shouldStepDown = false;
    float distanceForwardToStepDown = 0.0f;
    // Shapecast into the floor
    ShapecastHit hit = world.shapecast(snapDownRay, body.shapeHandle, orientation, filter);
    if (hit.isValid())
    {
        float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
        float slopeAngle = glm::acos(upAlongNormal);

        snappedDown = true;

        if (slopeAngle <= character.maxWalkableAngle)
        {
            body.position = body.position - world.UP_VECTOR * hit.t;
            body.velocity.y = 0.0f; // Testing
            character.groundNormal = hit.normal;
            character.groundState = PhysicsCharacter::GroundState::OnGround;
            character.jumping = false;

            if (hit.body->isKinematic)
                character.baseVelocity = hit.body->velocity;
            else
                character.baseVelocity = glm::vec3(0.0f);
        }
        else
        {
            //body.position = body.position - world.UP_VECTOR * hit.t;
            //body.velocity.y = 0.0f; // Testing
            //character.groundNormal = world.UP_VECTOR;
            // To be updated later
            character.groundState = PhysicsCharacter::GroundState::OnSteepGround;

            // We hit a ledge (stair / obstacle) or a slope
            glm::vec3 vectorForwardToStepDown = hit.pointA - body.position;
            vectorForwardToStepDown.y = 0.0f;
            distanceForwardToStepDown = capsule->radius - glm::length(vectorForwardToStepDown);
            // Get the factor of the 1/dot(hit.normal, desiredDirection);
            glm::vec3 nonVerticalNormal = glm::normalize(glm::vec3(hit.normal.x, 0.0f, hit.normal.z));
            float factor = 1.0f / glm::dot(nonVerticalNormal, desiredHorizontalDir);
            distanceForwardToStepDown *= factor;

            shouldStepDown = true;
        }
    }
    
    if (shouldStepDown)
    {
        Ray moveForwardRay = {
            .origin = body.position,
            .direction = desiredHorizontalDir,
            .maxDistance = distanceForwardToStepDown // a little padding
        };
        hit = world.shapecast(moveForwardRay, body.shapeHandle, orientation, filter);
        // We do NOT expect a hit at all, 
        if (hit.isValid()) return false;

        Ray stepDownRay = {
            .origin = moveForwardRay.origin + moveForwardRay.direction * moveForwardRay.maxDistance,
            .direction = -world.UP_VECTOR,
            .maxDistance = character.stepHeight
        };
        hit = world.shapecast(stepDownRay, body.shapeHandle, orientation, filter);
        // We are expecting a hit this time, if we do not, return false
        if (!hit.isValid())
        {
            hit = world.shapecast(stepDownRay, body.shapeHandle, orientation, filter);
            return false;
        }

        float upAlongNormal = glm::dot(hit.normal, world.UP_VECTOR);
        float slopeAngle = glm::acos(upAlongNormal);

        snappedDown = true;

        // only if we hit and the angle is walkable, we commit to that
        if (slopeAngle <= character.maxWalkableAngle)
        {
            body.position = stepDownRay.origin + stepDownRay.direction * stepDownRay.maxDistance * hit.t;
            body.velocity.y = 0.0f; // Testing
            character.groundNormal = hit.normal;
            character.groundState = PhysicsCharacter::GroundState::OnGround;
            character.jumping = false;

            if (hit.body->isKinematic)
                character.baseVelocity = hit.body->velocity;
            else
                character.baseVelocity = glm::vec3(0.0f);
        }
        
        
    }


    return snappedDown;
}




