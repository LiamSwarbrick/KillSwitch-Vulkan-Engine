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

// Macro to define an enum and automatically specialize the conversion functions
// VALUES is a list of X(enumName, value) calls
// e.g.
//#define COLLIDER_TYPE_VALUES(X) \
//    X(COL_TYPE_BOX)                \
//    X(COL_TYPE_SPHERE)             \
//    X(COL_TYPE_CAPSULE)

// Helper macros, to expand the X(entry) list to the correct syntax
#define ENUM_VALUE(entry) entry,
#define ENUM_TO_STRING_CASE(entry) case entry: return #entry;
#define STRING_TO_ENUM_CASE(entry) if (s == #entry) return entry;


#define DECLARE_ENUM(EnumName, VALUES)                                                  \
    typedef enum                                                                 \
    {                                                                                   \
        VALUES(ENUM_VALUE)                                                              \
    } EnumName;                                                                                  \
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



#endif // !CORE_UTILS_ENUM_REFLECTION_H
