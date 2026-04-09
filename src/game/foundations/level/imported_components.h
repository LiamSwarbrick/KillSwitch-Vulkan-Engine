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
    X(ImportedColliderType, BOX)                \
    X(ImportedColliderType, SPHERE)             \
    X(ImportedColliderType, CAPSULE)            

DECLARE_ENUM(ImportedColliderType, COLLIDER_TYPE_VALUES)

#define COLLIDER_FIELDS(F)    \
    F(ImportedColliderType, collider_type)   \
    F(float, radius)  \
    F(glm::vec3, half_widths) \
    F(float, height)

DECLARE_COMPONENT_STRUCT(ImportedCollider, COLLIDER_FIELDS)

#undef COLLIDER_TYPE_VALUES
#undef COLLIDER_FIELDS


#endif // !FOUNDATIONS_BLENDER_COMPONENTS_H
