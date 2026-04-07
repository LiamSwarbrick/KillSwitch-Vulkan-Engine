#ifndef CORE_UTILS_ENUM_REFLECTION_H
#define CORE_UTILS_ENUM_REFLECTION_H

#include "json_helpers.h"

#include <string>
#include <string_view>
#include <stdexcept>


// Deisgned this template to avoid having to use StringTo##EnumName, i thought StringToEnum<EnumName> would fit better
// StringToColliderType vs StringToEnum<ColliderType>
template<typename T>
T StringToEnum(std::string_view s) = delete;


// There are 2 ways of defining, either using 
// 1. ----- Copypasting the 3 defines and changing EnumName, then declaring VALUES
// 
// #define ENUM_VALUE_EnumName(e) ENUM_VALUE(EnumName, e)
// #define ENUM_TO_STRING_CASE_EnumName(e) ENUM_TO_STRING_CASE(EnumName, e)
// #define STRING_TO_ENUM_CASE_EnumName(e) STRING_TO_ENUM_CASE(EnumName, e)
// 
// Replacing "EnumName" for the actual name of the enum, and then defining the VALUES as single parameter...
// 
// #define COLLIDER_TYPE_VALUES(X) \
//     X(Box)        \
//     X(Sphere)     \
//     X(Capsule)
//
// or
// 2. ----- Add the enum name on VALUES like the following...
// #define COLLIDER_TYPE_VALUES(X) \
//     X(ColliderType, Box)        \
//     X(ColliderType, Sphere)     \
//     X(ColliderType, Capsule)


// For option 1. uncomment the next line, option 2. otherwise
//#define ENUM_EXTRA_FORWARD_DEFINES
#ifndef ENUM_EXTRA_FORWARD_DEFINES

// OPTION 2.

// Macro to define an enum and automatically specialize the conversion functions
// VALUES is a list of X(enumName, value) calls
// e.g.
//#define COLLIDER_TYPE_VALUES(X) \
//    X(ColliderType, Box)                \
//    X(ColliderType, Sphere)             \
//    X(ColliderType, Capsule)

// Helper macros, to expand the X(entry) list to the correct syntax
#define ENUM_VALUE(EnumName, entry) entry,
#define ENUM_TO_STRING_CASE(EnumName, entry) case EnumName::entry: return #entry;
#define STRING_TO_ENUM_CASE(EnumName, entry) if (s == #entry) return EnumName::entry;


#define DECLARE_ENUM(EnumName, VALUES)                                                  \
    enum class EnumName                                                                 \
    {                                                                                   \
        VALUES(ENUM_VALUE)                                                              \
    };                                                                                  \
                                                                                        \
    inline std::string_view EnumToString(EnumName e)                                    \
    {                                                                                   \
        switch (e) {                                                                    \
            VALUES(ENUM_TO_STRING_CASE)                                                 \
            default: return "Unknown";                                                  \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline EnumName StringToEnum<EnumName>(std::string_view s)                          \
    {                                                                                   \
        VALUES(STRING_TO_ENUM_CASE)                                                     \
        throw std::invalid_argument(std::string("Unknown " #EnumName ": ").append(s));  \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline EnumName rj_get<EnumName>(const rj::Value& v)                                \
    {                                                                                   \
        return StringToEnum<EnumName>(v.GetString());                                   \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline void rj_set<EnumName>(                                                       \
        rj::Value& v, const EnumName& x, rj::Document::AllocatorType& a)                \
    {                                                                                   \
        std::string_view str = EnumToString(x);                                         \
        v.SetString(str.data(), static_cast<rj::SizeType>(str.size()), a);              \
    }

#else

// OPTION 1.

// Same as option 2, but we will have to define the next 3 commented macros manually for each enum we declare
// #define ENUM_VALUE_EnumName(e) ENUM_VALUE(EnumName, e)
// #define ENUM_TO_STRING_CASE_EnumName(e) ENUM_TO_STRING_CASE(EnumName, e)
// #define STRING_TO_ENUM_CASE_EnumName(e) STRING_TO_ENUM_CASE(EnumName, e)
// 
// Replacing "EnumName" for the actual name of the enum, and then defining the VALUES as single parameter...
// 
// #define COLLIDER_TYPE_VALUES(X) \
//    X(Box)                \
//    X(Sphere)             \
//    X(Capsule)

#define ENUM_VALUE(EnumName, entry) entry,
#define ENUM_TO_STRING_CASE(EnumName, entry) case EnumName::entry: return #entry;
#define STRING_TO_ENUM_CASE(EnumName, entry) if (s == #entry) return EnumName::entry;

#define DECLARE_ENUM(EnumName, VALUES)                                                  \
    enum class EnumName                                                                 \
    {                                                                                   \
        VALUES(ENUM_VALUE_##EnumName)                                                              \
    };                                                                                  \
                                                                                        \
    inline std::string_view EnumToString(EnumName e)                                    \
    {                                                                                   \
        switch (e) {                                                                    \
            VALUES(ENUM_TO_STRING_CASE_##EnumName)                                                 \
            default: return "Unknown";                                                  \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline EnumName StringToEnum<EnumName>(std::string_view s)                          \
    {                                                                                   \
        VALUES(STRING_TO_ENUM_CASE_##EnumName)                                                     \
        throw std::invalid_argument(std::string("Unknown " #EnumName ": ").append(s));  \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline EnumName rj_get<EnumName>(const rj::Value& v)                                \
    {                                                                                   \
        return StringToEnum<EnumName>(v.GetString());                                   \
    }                                                                                   \
                                                                                        \
    template<>                                                                          \
    inline void rj_set<EnumName>(                                                       \
        rj::Value& v, const EnumName& x, rj::Document::AllocatorType& a)                \
    {                                                                                   \
        std::string_view str = EnumToString(x);                                         \
        v.SetString(str.data(), static_cast<rj::SizeType>(str.size()), a);              \
    }


#endif // ENUM_EXTRA_FORWARD_DEFINES





#endif // !CORE_UTILS_ENUM_REFLECTION_H
