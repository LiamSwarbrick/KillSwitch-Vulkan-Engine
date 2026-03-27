#ifndef CORE_COMPONENTS_H
#define CORE_COMPONENTS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "renderer/renderer.h"

struct C_Transform
{
    glm::vec3 position;
    glm::quat rotation;

    glm::mat4 matrix;
};

struct C_StaticMesh
{
    MeshPrefab mesh_prefab;
};

struct C_AnimatedMesh
{
    MeshPrefab mesh_prefab;
    // SkeletalAnimationState anim_state;  // <-TODO
};

// struct C_Mesh
// {
//     Mesh* data;
//     Asset* parent_asset;
// };

#endif //CORE_COMPONENTS_H