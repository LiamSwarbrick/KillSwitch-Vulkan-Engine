#ifndef FOUNDATIONS_COMPONENTS_H
#define FOUNDATIONS_COMPONENTS_H

// INCLUDE ALL MODULE COMPONENTS
#include "core/components.h"
// #include "physics/components.h"
// #include "renderer/components.h"

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

#endif // !FOUNDATIONS_COMPONENTS_H

