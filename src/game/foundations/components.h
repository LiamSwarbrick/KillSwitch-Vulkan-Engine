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

struct C_PlayerController
{
	// Movement helpers

	// Unsure if we should add a Slide state in a horror game (crouching while running), 
	// instead we can do like mario and crouch to stop faster (instead of running in the other direction)
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


	// DATA
	glm::vec3 position{ 0.0f }; // to linearly interpolate with the latest position
	glm::vec3 velocity{ 0.0f }; // character velocity (get from physics, modify, push to physics)

	bool jumping = false;
	// float jumpingCooldown = 0.0f; // we might not need this as: 1) we get feedback from physics engine. 2) we should jump on press not have constant input
	float jumpForce = 9.8f / 2.0f; // half earth's gravity
	bool isGrounded = false;

	// If we have crouch we might need to store either both heights OR both shape handles (capsules with different height)
	// We will see how this is tackled, for now i'll just test the movement of walk, sprint and crouch (as well as air- all of the previous)
	ShapeHandle standingShape;
	ShapeHandle crouchingShape;
	// Instead we could have the standingHeight, croushingHeight, currentHeight for a smooth transition in between, but im still not going to test this

	// Movement data
	MoveState state = MoveState::Walk; // Move State + Index to our movement profiles
	ForcedSlideInfo forcedSlide;

	MovementProfile groundProfiles[4] = {
		//maxSpeed  accel	friction
		{	3.0f,	 6.0f,	 12.0f }, // Walk
		{   5.0f,	10.0f,	 10.0f }, // Sprint
		{   1.5f,	 3.0f,	 18.0f }, // Crouch (max friction)
		{   8.0f,	 0.0f,	  3.0f }, // ForcedSlide
	};

	MovementProfile airProfiles[4] = {
		//maxSpeed  accel	friction
		{	3.0f,	 1.0f,	  1.5f }, // Air-Walk
		{   5.0f,	 1.5f,	  1.0f }, // Air-Sprint
		{   1.5f,	 0.5f,	  2.0f }, // Air-Crouch 
		{   8.0f,	 0.0f,	  0.5f }, // Air-Slide (what the helly) -> switch to air-walk/sprint/crouch
	};


	// Methods for movement (unsure if we should have them here OR in the system)

	// Movement function to get the current profile based on state and groundedState
	const MovementProfile& GetProfile() const
	{
		int idx = static_cast<int>(state);
		return isGrounded ? groundProfiles[idx] : airProfiles[idx];
	};
	
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
};

struct C_AIInput
{
	glm::vec3 target_position{ 0.0f };
	bool has_target = false;
};

struct C_Weapon
{
	EntityID weapon_entity = NULL_ENTITY; 
	glm::mat4 local_transform = glm::mat4(1.0f);
	bool equipped = false;

	const char* attach_bone_name;
	int attach_bone_index;
};

#endif // !FOUNDATIONS_COMPONENTS_H

