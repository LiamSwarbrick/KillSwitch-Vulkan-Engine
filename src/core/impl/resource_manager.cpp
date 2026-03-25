#include "core/resource_manager.h"

#include <assert.h>

static inline ResourceRecord*
get_record_mut(ResourceManager* manager, ResourceHandle handle)
{
    if (manager == NULL || handle == 0)
    {
        return NULL;
    }

    const u32 index = handle - 1;
    if (index >= manager->record_count)
    {
        return NULL;
    }

    return &manager->records[index];
}

static inline const ResourceRecord*
get_record_const(const ResourceManager* manager, ResourceHandle handle)
{
    return get_record_mut((ResourceManager*)manager, handle);
}

static void
copy_string_safe(char* dst, size_t dst_size, const char* src, const char* fallback)
{
    const char* s = src;
    if (s == NULL || s[0] == '\0')
    {
        s = fallback;
    }

    strncpy(dst, s, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void
ensure_capacity_for_one_more(ResourceManager* manager)
{
    assert(manager != NULL);

    if (manager->record_count < manager->record_capacity)
    {
        return;
    }

    const u32 old_capacity = manager->record_capacity;
    const u32 new_capacity = (old_capacity == 0) ? 16 : old_capacity * 2;

    ResourceRecord* new_records = (ResourceRecord*)L_realloc(
        manager->records,
        sizeof(ResourceRecord) * new_capacity,
        &manager->tt
    );

    memset(
        new_records + old_capacity,
        0,
        sizeof(ResourceRecord) * (new_capacity - old_capacity)
    );

    manager->records = new_records;
    manager->record_capacity = new_capacity;
}

static ResourceResidency
promote_residency(ResourceResidency current, ResourceResidency incoming)
{
    // 数值越小，生命周期越“强”
    // BOOT(0) > PERMANENT(1) > TRANSIENT(2)
    return (incoming < current) ? incoming : current;
}

static b32
load_record_now(ResourceManager* manager, ResourceRecord* record)
{
    if (manager == NULL || record == NULL)
    {
        return 0;
    }

    if (record->state == RESOURCE_LOAD_STATE_LOADED && record->loaded_data != NULL)
    {
        return 1;
    }

    if (record->loaded_data != NULL)
    {
        L_free(record->loaded_data, &manager->tt);
        record->loaded_data = NULL;
        record->byte_size = 0;
    }

    record->state = RESOURCE_LOAD_STATE_LOADING;

    u64 file_size = 0;
    void* bytes = L_load_binary_file(record->source_path, &file_size, &manager->tt);
    if (bytes == NULL)
    {
        record->state = RESOURCE_LOAD_STATE_FAILED;
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "ResourceManager: failed to load '%s' from '%s'",
            record->logical_name,
            record->source_path
        );
        return 0;
    }

    record->loaded_data = bytes;
    record->byte_size = file_size;
    record->state = RESOURCE_LOAD_STATE_LOADED;
    ++record->generation;

    SDL_Log(
        "ResourceManager: loaded [%u] '%s' (%" PRIu64 " bytes)",
        record->handle,
        record->logical_name,
        record->byte_size
    );

    return 1;
}

static void
unload_record(ResourceManager* manager, ResourceRecord* record)
{
    if (manager == NULL || record == NULL)
    {
        return;
    }

    if (record->loaded_data != NULL)
    {
        L_free(record->loaded_data, &manager->tt);
        record->loaded_data = NULL;
    }

    record->byte_size = 0;
    if (record->state != RESOURCE_LOAD_STATE_FAILED)
    {
        record->state = RESOURCE_LOAD_STATE_REGISTERED;
    }
}

static ResourceHandle
find_existing_handle(
    const ResourceManager* manager,
    const char* logical_name,
    const char* source_path
)
{
    if (manager == NULL)
    {
        return 0;
    }

    if (logical_name != NULL && logical_name[0] != '\0')
    {
        ResourceHandle by_name = ResourceManager_FindByName(manager, logical_name);
        if (by_name != 0)
        {
            return by_name;
        }
    }

    if (source_path != NULL && source_path[0] != '\0')
    {
        ResourceHandle by_path = ResourceManager_FindByPath(manager, source_path);
        if (by_path != 0)
        {
            return by_path;
        }
    }

    return 0;
}

ResourceManager
ResourceManager_Create(ResourceManagerCreateInfo create_info)
{
    ResourceManager manager = {};

    const char* tracker_name = create_info.debug_name;
    if (tracker_name == NULL || tracker_name[0] == '\0')
    {
        tracker_name = "ResourceManager";
    }

    manager.tt = init_per_thread_allocation_tracker(tracker_name);
    manager.record_capacity = (create_info.initial_capacity == 0) ? 16 : create_info.initial_capacity;
    manager.records = (ResourceRecord*)L_calloc(manager.record_capacity, sizeof(ResourceRecord), &manager.tt);

    SDL_Log(
        "ResourceManager '%s' created (capacity=%u)",
        manager.tt.tracker_name,
        manager.record_capacity
    );

    return manager;
}

void
ResourceManager_Destroy(ResourceManager* manager)
{
    if (manager == NULL)
    {
        return;
    }

    for (u32 i = 0; i < manager->record_count; ++i)
    {
        unload_record(manager, &manager->records[i]);
    }

    if (manager->records != NULL)
    {
        L_free(manager->records, &manager->tt);
        manager->records = NULL;
    }

    manager->record_count = 0;
    manager->record_capacity = 0;

    check_tracker_for_memory_leaks(&manager->tt);
}

ResourceHandle
ResourceManager_FindByName(const ResourceManager* manager, const char* logical_name)
{
    if (manager == NULL || logical_name == NULL || logical_name[0] == '\0')
    {
        return 0;
    }

    for (u32 i = 0; i < manager->record_count; ++i)
    {
        const ResourceRecord* record = &manager->records[i];
        if (strncmp(record->logical_name, logical_name, RESOURCE_NAME_MAX_LEN) == 0)
        {
            return record->handle;
        }
    }

    return 0;
}

ResourceHandle
ResourceManager_FindByPath(const ResourceManager* manager, const char* source_path)
{
    if (manager == NULL || source_path == NULL || source_path[0] == '\0')
    {
        return 0;
    }

    for (u32 i = 0; i < manager->record_count; ++i)
    {
        const ResourceRecord* record = &manager->records[i];
        if (strncmp(record->source_path, source_path, RESOURCE_PATH_MAX_LEN) == 0)
        {
            return record->handle;
        }
    }

    return 0;
}

const ResourceRecord*
ResourceManager_Get(const ResourceManager* manager, ResourceHandle handle)
{
    return get_record_const(manager, handle);
}

b32
ResourceManager_EnsureLoaded(ResourceManager* manager, ResourceHandle handle)
{
    ResourceRecord* record = get_record_mut(manager, handle);
    if (record == NULL)
    {
        return 0;
    }

    return load_record_now(manager, record);
}

ResourceHandle
ResourceManager_RequestFile(
    ResourceManager* manager,
    ResourceType type,
    ResourceResidency residency,
    const char* logical_name,
    const char* source_path
)
{
    if (manager == NULL || source_path == NULL || source_path[0] == '\0')
    {
        return 0;
    }

    ResourceHandle existing_handle = find_existing_handle(manager, logical_name, source_path);
    if (existing_handle != 0)
    {
        ResourceRecord* existing = get_record_mut(manager, existing_handle);
        SDL_assert(existing != NULL);

        if (existing->type != type && type != RESOURCE_TYPE_UNKNOWN)
        {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "ResourceManager: resource '%s' requested with different type (%d -> %d)",
                existing->logical_name,
                (int)existing->type,
                (int)type
            );
        }

        existing->residency = promote_residency(existing->residency, residency);
        ++existing->request_count;

        ResourceManager_EnsureLoaded(manager, existing_handle);
        return existing_handle;
    }

    ensure_capacity_for_one_more(manager);

    ResourceRecord* record = &manager->records[manager->record_count];
    memset(record, 0, sizeof(*record));

    record->handle = manager->record_count + 1;
    record->type = type;
    record->residency = residency;
    record->state = RESOURCE_LOAD_STATE_REGISTERED;
    record->request_count = 1;
    record->generation = 0;
    record->backend_rid = RESOURCE_BACKEND_RID_NONE;
    record->byte_size = 0;
    record->loaded_data = NULL;

    copy_string_safe(record->logical_name, sizeof(record->logical_name), logical_name, source_path);
    copy_string_safe(record->source_path, sizeof(record->source_path), source_path, "<missing-path>");

    ++manager->record_count;

    if (!load_record_now(manager, record))
    {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "ResourceManager: registered '%s' but initial load failed",
            record->logical_name
        );
    }

    return record->handle;
}

u32
ResourceManager_RequestBatch(
    ResourceManager* manager,
    const ResourceRequestDesc* requests,
    ResourceHandle* out_handles,
    u32 request_count
)
{
    if (manager == NULL || requests == NULL || request_count == 0)
    {
        return 0;
    }

    u32 success_count = 0;

    for (u32 i = 0; i < request_count; ++i)
    {
        ResourceHandle handle = ResourceManager_RequestFile(
            manager,
            requests[i].type,
            requests[i].residency,
            requests[i].logical_name,
            requests[i].source_path
        );

        if (out_handles != NULL)
        {
            out_handles[i] = handle;
        }

        if (handle != 0)
        {
            ++success_count;
        }
    }

    return success_count;
}

void
ResourceManager_Release(ResourceManager* manager, ResourceHandle handle)
{
    ResourceRecord* record = get_record_mut(manager, handle);
    if (record == NULL)
    {
        return;
    }

    if (record->request_count > 0)
    {
        --record->request_count;
    }

    if (record->request_count == 0 && record->residency == RESOURCE_RESIDENCY_TRANSIENT)
    {
        unload_record(manager, record);
    }
}

void
ResourceManager_Unload(ResourceManager* manager, ResourceHandle handle)
{
    ResourceRecord* record = get_record_mut(manager, handle);
    if (record == NULL)
    {
        return;
    }

    unload_record(manager, record);
}

void
ResourceManager_UnloadByResidency(
    ResourceManager* manager,
    ResourceResidency residency,
    b32 force_unload_even_if_referenced
)
{
    if (manager == NULL)
    {
        return;
    }

    for (u32 i = 0; i < manager->record_count; ++i)
    {
        ResourceRecord* record = &manager->records[i];
        if (record->residency != residency)
        {
            continue;
        }

        if (!force_unload_even_if_referenced && record->request_count > 0)
        {
            continue;
        }

        unload_record(manager, record);
    }
}

void
ResourceManager_SetBackendRID(ResourceManager* manager, ResourceHandle handle, u32 backend_rid)
{
    ResourceRecord* record = get_record_mut(manager, handle);
    if (record == NULL)
    {
        return;
    }

    record->backend_rid = backend_rid;
}

u32
ResourceManager_GetBackendRID(const ResourceManager* manager, ResourceHandle handle)
{
    const ResourceRecord* record = get_record_const(manager, handle);
    if (record == NULL)
    {
        return RESOURCE_BACKEND_RID_NONE;
    }

    return record->backend_rid;
}

void
ResourceManager_LogSummary(const ResourceManager* manager)
{
    if (manager == NULL)
    {
        return;
    }

    SDL_Log("--------------- ResourceManager Summary ---------------");
    SDL_Log(
        "tracker='%s' registered=%u capacity=%u",
        manager->tt.tracker_name,
        manager->record_count,
        manager->record_capacity
    );

    for (u32 i = 0; i < manager->record_count; ++i)
    {
        const ResourceRecord* record = &manager->records[i];
        SDL_Log(
            "[%u] name='%s' type=%d residency=%d state=%d refs=%u gen=%u bytes=%" PRIu64 " backend_rid=%u path='%s'",
            record->handle,
            record->logical_name,
            (int)record->type,
            (int)record->residency,
            (int)record->state,
            record->request_count,
            record->generation,
            record->byte_size,
            record->backend_rid,
            record->source_path
        );
    }

    SDL_Log("------------------------------------------------------");
}