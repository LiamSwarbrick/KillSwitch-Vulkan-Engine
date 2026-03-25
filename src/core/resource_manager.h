#ifndef ENGINE_RESOURCE_MANAGER_H
#define ENGINE_RESOURCE_MANAGER_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RESOURCE_NAME_MAX_LEN 96
#define RESOURCE_PATH_MAX_LEN 260
#define RESOURCE_BACKEND_RID_NONE 0xFFFFFFFFu

typedef u32 ResourceHandle;

typedef enum ResourceType
{
    RESOURCE_TYPE_UNKNOWN = 0,
    RESOURCE_TYPE_BINARY,
    RESOURCE_TYPE_SHADER_BYTECODE,
    RESOURCE_TYPE_TEXTURE,
    RESOURCE_TYPE_MESH,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_AUDIO,
    RESOURCE_TYPE_FONT,
    RESOURCE_TYPE_SKELETON,
    RESOURCE_TYPE_ANIMATION,
    RESOURCE_TYPE_COLLISION,
    RESOURCE_TYPE_WORLD
}
ResourceType;

typedef enum ResourceResidency
{
    RESOURCE_RESIDENCY_BOOT = 0,
    RESOURCE_RESIDENCY_PERMANENT,
    RESOURCE_RESIDENCY_TRANSIENT
}
ResourceResidency;

typedef enum ResourceLoadState
{
    RESOURCE_LOAD_STATE_UNREGISTERED = 0,
    RESOURCE_LOAD_STATE_REGISTERED,
    RESOURCE_LOAD_STATE_LOADING,
    RESOURCE_LOAD_STATE_LOADED,
    RESOURCE_LOAD_STATE_FAILED
}
ResourceLoadState;

typedef struct ResourceRecord
{
    ResourceHandle handle;
    ResourceType type;
    ResourceResidency residency;
    ResourceLoadState state;

    // 被多少模块 / 场景逻辑“持有”
    u32 request_count;

    // 每次重新加载成功后递增
    u32 generation;

    // 以后和 renderer / audio / physics backend 对接
    u32 backend_rid;

    u64 byte_size;
    void* loaded_data;

    char logical_name[RESOURCE_NAME_MAX_LEN];
    char source_path[RESOURCE_PATH_MAX_LEN];
}
ResourceRecord;

typedef struct ResourceRequestDesc
{
    ResourceType type;
    ResourceResidency residency;
    const char* logical_name;
    const char* source_path;
}
ResourceRequestDesc;

typedef struct ResourceManagerCreateInfo
{
    const char* debug_name;
    u32 initial_capacity;
}
ResourceManagerCreateInfo;

typedef struct ResourceManager
{
    ThreadAllocTracker tt;
    ResourceRecord* records;
    u32 record_count;
    u32 record_capacity;
}
ResourceManager;


// 生命周期
ResourceManager ResourceManager_Create(ResourceManagerCreateInfo create_info);
void ResourceManager_Destroy(ResourceManager* manager);

// 查找
ResourceHandle ResourceManager_FindByName(const ResourceManager* manager, const char* logical_name);
ResourceHandle ResourceManager_FindByPath(const ResourceManager* manager, const char* source_path);
const ResourceRecord* ResourceManager_Get(const ResourceManager* manager, ResourceHandle handle);

// 统一入口：模块都走这里请求资源
// 已存在则复用，不存在则注册并加载
ResourceHandle ResourceManager_RequestFile(
    ResourceManager* manager,
    ResourceType type,
    ResourceResidency residency,
    const char* logical_name,
    const char* source_path
);

// 批量请求：给 Scene::LoadLevel(GLTF_file) 用
u32 ResourceManager_RequestBatch(
    ResourceManager* manager,
    const ResourceRequestDesc* requests,
    ResourceHandle* out_handles,
    u32 request_count
);

// 显式保证已加载
b32 ResourceManager_EnsureLoaded(ResourceManager* manager, ResourceHandle handle);

// 释放一次引用
void ResourceManager_Release(ResourceManager* manager, ResourceHandle handle);

// 显式卸载
void ResourceManager_Unload(ResourceManager* manager, ResourceHandle handle);

// 按类别卸载
void ResourceManager_UnloadByResidency(
    ResourceManager* manager,
    ResourceResidency residency,
    b32 force_unload_even_if_referenced
);

// backend 对接
void ResourceManager_SetBackendRID(ResourceManager* manager, ResourceHandle handle, u32 backend_rid);
u32 ResourceManager_GetBackendRID(const ResourceManager* manager, ResourceHandle handle);

// Debug
void ResourceManager_LogSummary(const ResourceManager* manager);

#ifdef __cplusplus
}
#endif

#endif  // ENGINE_RESOURCE_MANAGER_H