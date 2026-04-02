#ifndef CORE_COMPONENTS_H
#define CORE_COMPONENTS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "renderer/render_types.h"
#include "core/assetsys.h"

struct C_Transform
{
    glm::vec3 position;
    glm::quat rotation;

    glm::mat4 matrix;
};

struct C_StaticMesh
{
	Mesh* mesh;
	Asset* parent_asset;
    MeshPrefab renderer_prefab;  // Loaded by renderer (empty before that)
};

struct C_AnimatedMesh
{
    Mesh* mesh;             
    Asset* asset;  
    MeshPrefab renderer_prefab;

    // animator states
    int currentAnimation;
    float animationTime;
    bool isPlaying;
    bool isLooping;

	// skeletal animation data
    uint32_t joint_count;
    glm::mat4* joint_matrices;
};

// struct C_Mesh
// {
//     Mesh* data;
//     Asset* parent_asset;
// };

#endif //CORE_COMPONENTS_H