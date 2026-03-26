#ifndef FOUNDATIONS_COMPONENTS_H
#define FOUNDATIONS_COMPONENTS_H

#include "core/assetsys.h"

// INCLUDE ALL MODULE COMPONENTS
#include "core/components.h"
// #include "physics/components.h"
// #include "renderer/components.h"


struct C_StaticMesh
{
	Mesh* mesh;
	Asset* parent_asset;
};

#endif // !FOUNDATIONS_COMPONENTS_H

