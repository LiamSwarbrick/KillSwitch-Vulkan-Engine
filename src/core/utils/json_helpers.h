#ifndef CORE_UTILS_JSON_HELPERS_H
#define CORE_UTILS_JSON_HELPERS_H

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include <string>


namespace rj = rapidjson;

// Get/Set template for RapidJSON. for automated (de-)serialization of structs
template<typename T> T      rj_get(const rj::Value& v);
template<typename T> void   rj_set(rj::Value& v, const T& val, rj::Document::AllocatorType& a);

template<> int          rj_get<int>(const rj::Value& v) { return v.GetInt(); }
template<> float        rj_get<float>(const rj::Value& v) { return v.GetFloat(); }
template<> double       rj_get<double>(const rj::Value& v) { return v.GetDouble(); }
template<> bool         rj_get<bool>(const rj::Value& v) { return v.GetBool(); }
template<> std::string  rj_get<std::string>(const rj::Value& v) { return v.GetString(); }
template<> glm::vec2    rj_get<glm::vec2>(const rj::Value& v)
{
    return { v[0].GetFloat(), v[1].GetFloat() };
}
template<> glm::vec3    rj_get<glm::vec3>(const rj::Value& v)
{
    return { v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat() };
}
template<> glm::vec4    rj_get<glm::vec4>(const rj::Value& v)
{
    return { v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat(), v[3].GetFloat() };
}
template<> glm::quat    rj_get<glm::quat>(const rj::Value& v)
{
    return { v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat(), v[3].GetFloat() };
}
// not doing mat3 hell naww just construct it with the data i don even know if you can matN in blender

template<> void rj_set<int>(rj::Value& v, const int& x, rj::Document::AllocatorType& a) { v.SetInt(x); }
template<> void rj_set<float>(rj::Value& v, const float& x, rj::Document::AllocatorType& a) { v.SetFloat(x); }
template<> void rj_set<double>(rj::Value& v, const double& x, rj::Document::AllocatorType& a) { v.SetDouble(x); }
template<> void rj_set<bool>(rj::Value& v, const bool& x, rj::Document::AllocatorType& a) { v.SetBool(x); }
template<> void rj_set<std::string>(rj::Value& v, const std::string& x, rj::Document::AllocatorType& a)
{
    v.SetString(x.c_str(), static_cast<rj::SizeType>(x.size()), a);
}
template<> void rj_set<glm::vec2>(rj::Value& v, const glm::vec2& x, rj::Document::AllocatorType& a)
{
    v.SetArray();
    v.PushBack(x.x, a).PushBack(x.y, a);
}
template<> void rj_set<glm::vec3>(rj::Value& v, const glm::vec3& x, rj::Document::AllocatorType& a)
{
    v.SetArray();
    v.PushBack(x.x, a).PushBack(x.y, a).PushBack(x.z, a);
}
template<> void rj_set<glm::vec4>(rj::Value& v, const glm::vec4& x, rj::Document::AllocatorType& a)
{
    v.SetArray();
    v.PushBack(x.x, a).PushBack(x.y, a).PushBack(x.z, a).PushBack(x.w, a);
}


#endif // !CORE_UTILS_JSON_HELPERS_H

