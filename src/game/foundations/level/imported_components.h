#ifndef FOUNDATIONS_IMPORTED_COMPONENTS_H
#define FOUNDATIONS_IMPORTED_COMPONENTS_H

#include "core/imported_components.h"

#include "core/utils/enum_reflection.h"
#include "core/utils/struct_reflection.h"
#include "core/utils/json_helpers.h"

// ---------------------------------
// ---------- READ THIS ------------
// ---------------------------------
// 
// Difference with core/imported_components.h is these components SHOULD come all from the json
// 
// Some ImportedComponents declared here are going to be using reflective declaration using macros,
// as well as automated (de-)serialization of their attributes from RapidJSON (defined in core).
// 
// They NEED to match the data that's coming from the GLTF.extra["_ecs"] section
// That means the names of the variables, as well as their expected values, such as an enum, or a glm::vec3, etc.
// Enum declarations also need to be reflective, there will be an example here, using ImportedCollider for reflective,
// and ImportedTransform for non-reflective structs


// ---------------------------------
// ---- HERE REFLECTIVE STRUCTS ----
// ---------------------------------
// (and enums for the structs), or they could be defined elsewhere, here for now idk

// Example ahead
// ---- IMPORTED COLLIDER -----

// Notice you have to add the type of the enum on every value,
// For an alternative, read option 1 on core/utils/enum_reflection.h
#define COLLIDER_TYPE_VALUES(X) \
    X(ImportedColliderType, COL_TYPE_NONE)               \
    X(ImportedColliderType, COL_TYPE_BOX)                \
    X(ImportedColliderType, COL_TYPE_SPHERE)             \
    X(ImportedColliderType, COL_TYPE_CAPSULE)            \
    X(ImportedColliderType, COL_TYPE_CONVEX_HULL)                

DECLARE_ENUM(ImportedColliderType, COLLIDER_TYPE_VALUES)

#define RIGIDBODY_FIELDS(F)    \
    F(float, mass)   \
    F(float, gravity_scale)   \
    F(float, damping)   \
    F(int, force_layers)   \
    F(glm::vec3, collider_position_offset)   \
    F(glm::quat, collider_rotation_offset)   \
    F(ImportedColliderType, collider_type)   \
    F(float, radius)  \
    F(glm::vec3, half_widths) \
    F(float, height) \
    F(bool, is_static) \
    F(bool, is_kinematic) \
    F(bool, is_character) \
    F(bool, is_trigger)

DECLARE_COMPONENT_STRUCT(ImportedRigidbody, RIGIDBODY_FIELDS)

#undef COLLIDER_TYPE_VALUES
#undef RIGIDBODY_FIELDS



#endif // !FOUNDATIONS_BLENDER_COMPONENTS_H
