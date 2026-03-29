#ifndef CORE_UTILS_STRUCT_REFLECTION_H
#define CORE_UTILS_STRUCT_REFLECTION_H

#include "json_helpers.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept>

namespace rj = rapidjson;


// We could not do the "To" function as template due overloading
template<typename T>
rj::Value StructToRapidJsonValue(const T& s, rj::Document::AllocatorType& alloc) = delete;

template<typename T>
T StructFromRapidJsonValue(const rj::Value& obj) = delete;


// Same as the enum definition, but we will use DECLARE_COMPONENT_STRUCT(StructName, FIELDS) if we want RapidJSON (de-)serialization
// 'F' for field
//#define PLAYER_FIELDS(F)    \
//    F(std::string,  name)   \
//    F(int,          level)  \
//    F(float,        health) \
//    F(bool,         alive)

#define DECLARE_STRUCT(StructName, FIELDS)  \
    struct StructName                       \
    {                                       \
        FIELDS(STRUCT_MEMBER)               \
    };

#define DECLARE_STRUCT_JSON_UTILS(StructName, FIELDS)                           \
    template<>                                                                  \
    inline rj::Value StructToRapidJsonValue<StructName>(                        \
        const StructName& s, rj::Document::AllocatorType& alloc)                \
    {                                                                           \
        /* No obj(#StructName,...) to allow different naming conventions */     \
        rj::Value obj(rj::kObjectType);                                         \
        FIELDS(STRUCT_TO_JSON)                                                  \
        return obj;                                                             \
    }                                                                           \
                                                                                \
    template<>                                                                  \
    inline StructName StructFromRapidJsonValue<StructName>(const rj::Value& obj)\
    {                                                                           \
        StructName s{};                                                         \
        FIELDS(STRUCT_FROM_JSON)                                                \
        return s;                                                               \
    }

    
#define DECLARE_COMPONENT_STRUCT(StructName, FIELDS)                            \
    DECLARE_STRUCT(StructName, FIELDS)                                          \
    DECLARE_STRUCT_JSON_UTILS(StructName, FIELDS)

// Macro helpers for the designed Fields
#define STRUCT_MEMBER(type, name) type name;

#define STRUCT_TO_JSON(type, name)                                              \
    {                                                                           \
        rj::Value key(#name, alloc);                                            \
        rj::Value val;                                                          \
        rj_set<type>(val, s.name, alloc);                                         \
        obj.AddMember(key, val, alloc);                                         \
    }

#define STRUCT_FROM_JSON(type, name)                                            \
    if (obj.HasMember(#name) && !obj[#name].IsNull())                           \
        s.name = rj_get<type>(obj[#name]);


#endif // !CORE_UTILS_STRUCT_REFLECTION_H

