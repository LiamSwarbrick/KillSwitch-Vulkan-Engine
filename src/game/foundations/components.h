#ifndef FOUNDATIONS_COMPONENTS_H
#define FOUNDATIONS_COMPONENTS_H

// INCLUDE ALL MODULE COMPONENTS
#include "core/components.h"
// #include "physics/components.h"
// #include "renderer/components.h"
// #include "core/animation/components.h"

#include "core/assetsys.h"
#include "glm/glm.hpp"


// TODO: change definition to physics/components.h
enum class ColliderType 
{
	Box = 0,
	Sphere,
	Capsule,
	ABBB,
};

struct C_Collider
{
	ColliderType type;

	union
	{
		struct
		{
			float radius;
		} sphere;

		struct
		{
			glm::vec3 halfWidths;
		} box;

		struct
		{
			float radius;
			float height;
		} capsule;
	};
};

struct C_CharacterController
{
	glm::vec3 velocity{ 0.0f };
	float move_speed = 5.0f;

	glm::vec3 target_position{ 0.0f };
	bool jumping = false;

	bool is_grounded = false;
};

struct C_PlayerInput
{
};

struct C_AIInput
{
	glm::vec3 target_position{ 0.0f };
	bool has_target = false;
};

#endif // !FOUNDATIONS_COMPONENTS_H

