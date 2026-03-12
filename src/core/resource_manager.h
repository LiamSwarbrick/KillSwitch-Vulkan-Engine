#ifndef ENGINE_RESOURCE_MANAGER_H
#define ENGINE_RESOURCE_MANAGER_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RESOURCE_NAME_MAX_LEN 64
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
    RESOURCE_TYPE_COLLISION,
    RESOURCE_TYPE_WORLD
}
ResourceType;

typedef enum ResourceLoadState
{
    RESOURCE_LOAD_STATE_UNREGISTERED = 0,
    RESOURCE_LOAD_STATE_REGISTERED,
    RESOURCE_LOAD_STATE_LOADED,
    RESOURCE_LOAD_STATE_FAILED
}
ResourceLoadState;

typedef struct ResourceRecord
{
    ResourceHandle handle;      // Stable opaque handle. 0 is invalid.
    ResourceType type;
    ResourceLoadState state;

    u32 generation;             // Increased whenever bytes are reloaded.
    u32 backend_rid;            // Future bridge into renderer / GPU resource IDs.
    u64 byte_size;
    void* loaded_data;

    char logical_name[RESOURCE_NAME_MAX_LEN];
    char source_path[RESOURCE_PATH_MAX_LEN];
}
ResourceRecord;

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

ResourceManager ResourceManager_Create(ResourceManagerCreateInfo create_info);
void ResourceManager_Destroy(ResourceManager* manager);

ResourceHandle ResourceManager_RegisterFile(
    ResourceManager* manager,
    ResourceType type,
    const char* logical_name,
    const char* source_path
);

ResourceHandle ResourceManager_FindByName(const ResourceManager* manager, const char* logical_name);
const ResourceRecord* ResourceManager_Get(const ResourceManager* manager, ResourceHandle handle);
ResourceRecord* ResourceManager_GetMutable(ResourceManager* manager, ResourceHandle handle);

b32 ResourceManager_LoadBinary(ResourceManager* manager, ResourceHandle handle);
void ResourceManager_Unload(ResourceManager* manager, ResourceHandle handle);
void ResourceManager_SetBackendRID(ResourceManager* manager, ResourceHandle handle, u32 backend_rid);
void ResourceManager_LogSummary(const ResourceManager* manager);

#ifdef __cplusplus
}
#endif

#endif  // ENGINE_RESOURCE_MANAGER_H