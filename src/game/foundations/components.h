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

struct C_CharacterController
{
	glm::vec3 velocity{ 0.0f };
	float move_speed = 3.0f;

	glm::vec3 target_position{ 0.0f };
	bool jumping = false;
	float jumping_cooldown = 0.0f;

	bool is_grounded = false;
};

struct C_PlayerInput
{
	bool move_forward = false;
	bool move_backward = false;
	bool move_left = false;
	bool move_right = false;
	bool jump = false;
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

	int attach_bone_index = -1;
};

#endif // !FOUNDATIONS_COMPONENTS_H

