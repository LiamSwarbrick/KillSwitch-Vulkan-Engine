#ifndef CORE_COMPONENTS_H
#define CORE_COMPONENTS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct C_Transform
{
    glm::vec3 position;
    glm::quat rotation;

    glm::mat4 matrix;
};

#endif //CORE_COMPONENTS_H