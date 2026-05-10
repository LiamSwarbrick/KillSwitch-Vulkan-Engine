#ifndef FOUNDATIONS_COMPONENTS_H
#define FOUNDATIONS_COMPONENTS_H

// INCLUDE ALL MODULE COMPONENTS
#include "core/components.h"
#include "core/ecs.h"  
#include "physics/components.h"
// #include "renderer/components.h"
// #include "core/animation/components.h"

#include "core/assetsys.h"
#include "glm/glm.hpp"

enum MoveState
{
	Walk = 0,
	Sprint,
	Crouch, // Crouch but could be slow walk if we do not have crouch animations
	ForcedSlide // Optional, but we could forcefully slide down unwalkable surfaces, or just let the physics engine slide us
};

struct MovementProfile
{
	float maxSpeed;
	float acceleration;
	float friction;
};

struct ForcedSlideInfo
{
	glm::vec3 lockedDir = glm::vec3(0.0f, 0.0f, 0.0f); // HORIZONTAL (not vertical please) direction locked at slide start 
	float duration = 0.0f; // slide duration
};

//struct C_PlayerController
//{
//
//	// Movement helpers
//
//	// Unsure if we should add a Slide state in a horror game (crouching while running), 
//	// instead we can do like mario and crouch to stop faster (instead of running in the other direction)
//	enum MoveState
//	{
//		Walk = 0,
//		Sprint,
//		Crouch, // Crouch but could be slow walk if we do not have crouch animations
//		ForcedSlide // Optional, but we could forcefully slide down unwalkable surfaces, or just let the physics engine slide us
//	};
//
//	struct MovementProfile
//	{
//		float maxSpeed;
//		float acceleration;
//		float friction;
//	};
//
//	struct ForcedSlideInfo
//	{
//		glm::vec3 lockedDir = glm::vec3(0.0f, 0.0f, 0.0f); // HORIZONTAL (not vertical please) direction locked at slide start 
//		float duration = 0.0f; // slide duration
//	};
//
//
//	// DATA
//	glm::vec3 position{ 0.0f }; // to linearly interpolate with the latest position
//	glm::vec3 velocity{ 0.0f }; // character velocity (get from physics, modify, push to physics)
//
//	bool jumping = false;
//	// float jumpingCooldown = 0.0f; // we might not need this as: 1) we get feedback from physics engine. 2) we should jump on press not have constant input
//	float jumpForce = 9.8f / 2.0f; // half earth's gravity
//	bool isGrounded = false;
//
//	// for animations
//	float idleTimer = 0.0f; 
//	bool hitReacting = false;
//	float hitReactTimer = 0.0f;
//
//	// If we have crouch we might need to store either both heights OR both shape handles (capsules with different height)
//	// We will see how this is tackled, for now i'll just test the movement of walk, sprint and crouch (as well as air- all of the previous)
//	ShapeHandle standingShape;
//	ShapeHandle crouchingShape;
//	// Instead we could have the standingHeight, croushingHeight, currentHeight for a smooth transition in between, but im still not going to test this
//
//	// Movement data
//	MoveState state = MoveState::Walk; // Move State + Index to our movement profiles
//	ForcedSlideInfo forcedSlide;
//
//	MovementProfile groundProfiles[4] = {
//		//maxSpeed  accel	friction
//		{	3.0f,	 6.0f,	 12.0f }, // Walk
//		{   5.0f,	10.0f,	 10.0f }, // Sprint
//		{   1.5f,	 3.0f,	 18.0f }, // Crouch (max friction)
//		{   8.0f,	 0.0f,	  3.0f }, // ForcedSlide
//	};
//
//	MovementProfile airProfiles[4] = {
//		//maxSpeed  accel	friction
//		{	3.0f,	 1.0f,	  1.5f }, // Air-Walk
//		{   5.0f,	 1.5f,	  1.0f }, // Air-Sprint
//		{   1.5f,	 0.5f,	  2.0f }, // Air-Crouch 
//		{   8.0f,	 0.0f,	  0.5f }, // Air-Slide (what the helly) -> switch to air-walk/sprint/crouch
//	};
//
//
//	// Methods for movement (unsure if we should have them here OR in the system)
//
//	// Movement function to get the current profile based on state and groundedState
//	const MovementProfile& GetProfile() const
//	{
//		int idx = static_cast<int>(state);
//		return isGrounded ? groundProfiles[idx] : airProfiles[idx];
//	};
//	
//};

struct C_Faction
{
	enum Type
	{
		Player,
		Zombie,
		// To add more, like Neutral etc
	};

	Type type;
};

struct C_MovementInput
{
	// Written by InputSystem
	glm::vec3 desiredDir{ 0.0f };
	float moveAmount = 0.0f;

	bool wantsJump = false;
	bool wantsRun = false;
	bool wantsCrouch = false;
	bool wantsAim = false;
	glm::vec3 aimDir{ 0.0f }; // For now, to rotate optionally (this should be in an animation input component or something)

	// If we want dynamic combat just add some sort of slow down multiplier when attacking
	float combatFactor = 0.0f;
};

struct C_MovementStats
{
	// Move stats, defaulted or changed by RoundSystem (to make zombies faster, etc), 
	// Read from Movement System
	std::vector<std::pair<MoveState, MovementProfile>> groundProfiles;
	std::vector<std::pair<MoveState, MovementProfile>> airProfiles;

	float jumpForce = 9.8f / 2.0f;

	const MovementProfile& GetGroundProfile(MoveState desiredState) const
	{
		for (const auto& [state, profile] : groundProfiles)
		{
			if (state == desiredState) return profile;
		}

		return groundProfiles[0].second;
	}

	const MovementProfile& GetAirProfile(MoveState desiredState) const
	{
		for (const auto& [state, profile] : airProfiles)
		{
			if (state == desiredState) return profile;
		}

		return groundProfiles[0].second;
	}

	static C_MovementStats DefaultPlayerStats()
	{
		return C_MovementStats {
			.groundProfiles = {
				//	MoveState				 maxSpeed   accel	friction
				{MoveState::Walk,			{	3.0f,	 6.0f,	 12.0f }}, // Walk
				{MoveState::Sprint,			{   5.0f,	10.0f,	 10.0f }}, // Sprint
				{MoveState::Crouch,			{   1.5f,	 3.0f,	 18.0f }}, // Crouch (max friction)
				{MoveState::ForcedSlide,	{   8.0f,	 0.0f,	  3.0f }}, // ForcedSlide
			},
			.airProfiles= {
				//	MoveState				 maxSpeed   accel	friction
				{MoveState::Walk,			{	3.0f,	 5.0f,	  1.5f }}, // Air-Walk
				{MoveState::Sprint,			{   5.0f,	 6.5f,	  1.0f }}, // Air-Sprint
				{MoveState::Crouch,			{   1.5f,	 1.5f,	  2.0f }}, // Air-Crouch 
				{MoveState::ForcedSlide,	{   8.0f,	 0.0f,	  0.5f }}, // Air-Slide (what the helly) -> switch to air-walk/sprint/crouch
			},
		};
	}

	static C_MovementStats DefaultZombieStats()
	{
		return C_MovementStats {
			.groundProfiles = {
				//	MoveState				 maxSpeed   accel	friction
				{MoveState::Walk,			{	3.0f,	 6.0f,	 12.0f }}, // Walk
				{MoveState::Sprint,			{   5.0f,	10.0f,	 10.0f }}, // Sprint
				{MoveState::Crouch,			{   1.5f,	 3.0f,	 18.0f }}, // This can count as slowwalk
				{MoveState::ForcedSlide,	{   8.0f,	 0.0f,	  3.0f }}, // ForcedSlide
			},
			.airProfiles = {
				//	MoveState				 maxSpeed   accel	friction
				{MoveState::Walk,			{	3.0f,	 1.0f,	  1.5f }}, // Air-Walk
				{MoveState::Sprint,			{   5.0f,	 1.5f,	  1.0f }}, // Air-Sprint
				{MoveState::Crouch,			{   1.5f,	 0.5f,	  2.0f }}, // Air-Crouch 
				{MoveState::ForcedSlide,	{   8.0f,	 0.0f,	  0.5f }}, // Air-Slide (what the helly) -> switch to air-walk/sprint/crouch
			},
		};
	}
};

// We could join C_MovementStats and C_MovementInfo together

// Movement information
struct C_MovementInfo
{
	// Movement information written by the Movement System
	// DATA
	glm::vec3 position{ 0.0f }; // to linearly interpolate with the latest position
	glm::vec3 velocity{ 0.0f }; // character velocity (get from physics, modify, push to physics)

	// Movement data
	MoveState state = MoveState::Walk; // Move State + Index to our movement profiles
	ForcedSlideInfo forcedSlide;

	bool isMoving = false;
	bool isGrounded = false;
	bool isJumping = false;

	float idleTimer = 0.0f;
};

struct C_CombatInput
{
	// Modified by an InputSystem
	bool wantsMelee = false;
	bool wantsRanged = false;
	glm::vec3 aimDir{ 0.0f };

	// Modified by the CombatSystem
	bool isAttacking = false;
};


struct C_WeaponSocket
{
	EntityID weapon_entity = NULL_ENTITY;
	glm::mat4 local_transform = glm::mat4(1.0f);
	bool equipped = false;

	const char* attach_bone_name;
	int attach_bone_index;
};

struct C_WeaponMelee
{
	// Don't know what weapon types could exist in a horror game
	// Type should be for the combat system (depending on the weapon we could have different attack patterns)
	enum Type
	{
		Knuckle,
		Knife,
		Crowbar,
		Sword, //lmao
	};

	float range = 1.0f;
	float damage = 1.0f;
	bool hasDurability = false;
	float maxDurability = 0.0f;
	float currentDurabiliy = 0.0f;
};

struct C_WeaponRanged
{
	enum Type
	{
		Pistol,
		Shotgun,
		Rifle,
	};

	enum FiringMode
	{
		Semi,
		Burst,
		Auto,
	};

	Type type;
	FiringMode firingMode;

	float damage = 0.0f;
	short maxBullets = 0;
	short currentBullets = 0;

	float lastTimeSinceShot = 0.0f;
	int shotsPerFire = 0; // shots per fire (for bursts or any other semi that shoots multiple things (double barrel shotgun or anything idk))
	float shootMaxCooldown = 0.0f; // the time between each shot

	// Extra goofy shit
	float dispersionRecoveryCooldown = 0.0f;
	std::function<void()> dispersionPattern; // idk if this would even work

	static C_WeaponRanged DefaultPistol()
	{
		return C_WeaponRanged{
			.type = Pistol,
			.firingMode = Semi,
			.damage = 100.0f, // Assume 100 health is a zombie's
			.maxBullets = 1,
			.currentBullets = 1,
			.lastTimeSinceShot = 0.0f,
			.shotsPerFire = 1,
			.shootMaxCooldown = 1.0f,
			.dispersionRecoveryCooldown = 1.5f
		};
	}
};



struct C_PlayerInput
{
	bool move_forward = false;
	float forward = 0.0f;
	bool move_backward = false;
	float backward = 0.0f;
	bool move_left = false;
	float left = 0.0f;
	bool move_right = false;
	float right = 0.0f;
	bool jump = false;
	bool crouch = false; // or slow walk
	bool run = false;
	bool aim = false;
	bool attack = false;
};

struct C_AIInput
{
	glm::vec3 target_position{ 0.0f };
	bool has_target = false;
};

// This should be changed by the game (if procedural generation) or added manually in the blender script if manual creation of levels
struct C_EnemyAIStats
{
	// Vision & alert
	float visionDistance = 10.0f;
	float visionMaxAngle = glm::radians(90.0f); // The max angle to where we're looking

	float alertDistance = 6.0f; // We probably should NOT have these, but the player having alertDistances on walk, run, jump and then read them from the zombie

	// Attack ranges
	float attackDistance = 1.0f;

	// Patrol (unsure where the patrol thingy should go. should it go on stats?, we could have target be the current patrol point)
	std::vector<glm::vec3> patrolPoints;
	int currentPatrolIndex = 0;
	float patrolWaitTime = 2.0f;
	float patrolWaitTimer = 0.0f;

	// Turn speed (angles/radians per second or sum'n idk)
	float turnSpeed = glm::radians(360.0f); 

	float finishAttackTime = 1.0f;
	float attackCooldownTime = 0.5f; // Possibly unused
};

// To subdivide between the AIInfo and the AIStats (all ranges: vision distance, alert distance, attack range)
struct C_EnemyAIInfo
{
	enum State
	{
		Idle,
		Patrol,
		Alerted, // Until we implement a more complex AI i don't think this is going to be used
		Chase,
		Search,
		Attack,
		Staggered, // When pushed by our melee?
		Dead, // Just in case, if we kill a zombie make it dead if we had ragdolls or whatever, but we might aswell despawn for now
	};

	State currentState = State::Idle;
	State fallbackState = State::Idle;

	// Target 
	// (for Idle: nothing
	// (for Patrol: the current patrolPoint
	// (for Alerted : the place we have to look at, 
	// (for Chase : the current player's position we should run at (if out of sight the target will be our last seen target), 
	// (for Attack : the position we are attacking at
	// (for Staggered: nothing or the place we should look at after being staggered
	// (for Dead: nothing
	glm::vec3 target{ 0.0f };
	bool hasTarget = false;

	// For Search mode (go to target -> reach target or max timer, fall back to idle)
	bool hasReachedTarget = false;
	float reachTheTargetTimer = 0.0f;
	float reachTheTargetMaxTime = 5.0f;

	// For alerted mode
	float alertedTimer = 0.0f;
	float alertedMaxTime = 5.0f;

	// For Chasing / Attack, to check the active target first (say the zombie is dumb), and if the target falls off range then fallback to check for other players
	EntityID activeTargetID = NULL_ENTITY;

	// Timers
	float attackTimer = 0.0f;

	// Important addition (TODO: add turn speed calculations to turning)
	glm::vec3 targetLookDirToLerp{ 0.0f };
};


#endif // !FOUNDATIONS_COMPONENTS_H
