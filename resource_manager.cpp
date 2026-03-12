#include "core/resource_manager.h"
 
#include <assert.h>
 
static inline ResourceRecord*
get_record_from_handle(ResourceManager* manager, ResourceHandle handle)
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
get_record_from_handle_const(const ResourceManager* manager, ResourceHandle handle)
{
   return get_record_from_handle((ResourceManager*)manager, handle);
}
 
static void
ensure_capacity_for_one_more(ResourceManager* manager)
{
   assert(manager != NULL);
 
   if (manager->record_count < manager->record_capacity)
   {
       return;
   }
 
   u32 new_capacity = (manager->record_capacity == 0) ? 16 : manager->record_capacity * 2;
   ResourceRecord* new_records = (ResourceRecord*)L_realloc(
       manager->records,
       sizeof(ResourceRecord) * new_capacity,
       &manager->tt
   );
 
   memset(
       new_records + manager->record_capacity,
       0,
       sizeof(ResourceRecord) * (new_capacity - manager->record_capacity)
   );
 
   manager->records = new_records;
   manager->record_capacity = new_capacity;
}
 
static void
copy_string_with_fallback(char* dst, size_t dst_size, const char* src, const char* fallback)
{
   assert(dst != NULL);
   assert(dst_size > 0);
 
   const char* safe_src = src;
   if (safe_src == NULL || safe_src[0] == '\0')
   {
       safe_src = fallback;
   }
 
   strncpy(dst, safe_src, dst_size - 1);
   dst[dst_size - 1] = '\0';
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
 
   SDL_Log("ResourceManager '%s' created with capacity=%u",
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
       ResourceRecord* record = &manager->records[i];
       if (record->loaded_data)
       {
           L_free(record->loaded_data, &manager->tt);
           record->loaded_data = NULL;
       }
   }
 
   if (manager->records)
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
ResourceManager_RegisterFile(ResourceManager* manager, ResourceType type, const char* logical_name, const char* source_path)
{
   if (manager == NULL || source_path == NULL || source_path[0] == '\0')
   {
       return 0;
   }
 
   const char* safe_name = logical_name;
   if (safe_name == NULL || safe_name[0] == '\0')
   {
       safe_name = source_path;
   }
 
   ResourceHandle existing = ResourceManager_FindByName(manager, safe_name);
   if (existing != 0)
   {
       ResourceRecord* existing_record = ResourceManager_GetMutable(manager, existing);
       SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
           "Resource '%s' already registered. Updating source path to '%s' instead of adding duplicate.",
           safe_name,
           source_path
       );
       copy_string_with_fallback(
           existing_record->source_path,
           sizeof(existing_record->source_path),
           source_path,
           "<missing-path>"
       );
       return existing;
   }
 
   ensure_capacity_for_one_more(manager);
 
   ResourceRecord* record = &manager->records[manager->record_count];
   memset(record, 0, sizeof(*record));
 
   record->handle = manager->record_count + 1;
   record->type = type;
   record->state = RESOURCE_LOAD_STATE_REGISTERED;
   record->generation = 0;
   record->backend_rid = RESOURCE_BACKEND_RID_NONE;
   record->byte_size = 0;
   record->loaded_data = NULL;
 
   copy_string_with_fallback(record->logical_name, sizeof(record->logical_name), safe_name, "unnamed_resource");
   copy_string_with_fallback(record->source_path, sizeof(record->source_path), source_path, "<missing-path>");
 
   ++manager->record_count;
 
   SDL_Log("Registered resource [%u] '%s' -> %s",
       record->handle,
       record->logical_name,
       record->source_path
   );
 
   return record->handle;
}
 
const ResourceRecord*
ResourceManager_Get(const ResourceManager* manager, ResourceHandle handle)
{
   return get_record_from_handle_const(manager, handle);
}
 
ResourceRecord*
ResourceManager_GetMutable(ResourceManager* manager, ResourceHandle handle)
{
   return get_record_from_handle(manager, handle);
}
 
b32
ResourceManager_LoadBinary(ResourceManager* manager, ResourceHandle handle)
{
   ResourceRecord* record = ResourceManager_GetMutable(manager, handle);
   if (record == NULL)
   {
       return 0;
   }
 
   if (record->loaded_data)
   {
       L_free(record->loaded_data, &manager->tt);
       record->loaded_data = NULL;
       record->byte_size = 0;
   }
 
   u64 file_size = 0;
   void* bytes = L_load_binary_file(record->source_path, &file_size, &manager->tt);
   if (bytes == NULL)
   {
       record->state = RESOURCE_LOAD_STATE_FAILED;
       SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
           "Failed to load resource '%s' from '%s'",
           record->logical_name,
           record->source_path
       );
       return 0;
   }
 
   record->loaded_data = bytes;
   record->byte_size = file_size;
   record->state = RESOURCE_LOAD_STATE_LOADED;
   ++record->generation;
 
   SDL_Log("Loaded resource [%u] '%s' (%" PRIu64 " bytes)",
       record->handle,
       record->logical_name,
       record->byte_size
   );
 
   return 1;
}
 
void
ResourceManager_Unload(ResourceManager* manager, ResourceHandle handle)
{
   ResourceRecord* record = ResourceManager_GetMutable(manager, handle);
   if (record == NULL || record->loaded_data == NULL)
   {
       return;
   }
 
   L_free(record->loaded_data, &manager->tt);
   record->loaded_data = NULL;
   record->byte_size = 0;
   record->state = RESOURCE_LOAD_STATE_REGISTERED;
}
 
void
ResourceManager_SetBackendRID(ResourceManager* manager, ResourceHandle handle, u32 backend_rid)
{
   ResourceRecord* record = ResourceManager_GetMutable(manager, handle);
   if (record == NULL)
   {
       return;
   }
 
   record->backend_rid = backend_rid;
}
 
void
ResourceManager_LogSummary(const ResourceManager* manager)
{
   if (manager == NULL)
   {
       return;
   }
 
   SDL_Log("---------------- Resource Manager Summary ----------------");
   SDL_Log("tracker='%s', registered=%u, capacity=%u",
       manager->tt.tracker_name,
       manager->record_count,
       manager->record_capacity
   );
 
   for (u32 i = 0; i < manager->record_count; ++i)
   {
       const ResourceRecord* record = &manager->records[i];
       SDL_Log("[%u] name='%s' type=%d state=%d bytes=%" PRIu64 " backend_rid=%u path='%s'",
           record->handle,
           record->logical_name,
           (int)record->type,
           (int)record->state,
           record->byte_size,
           record->backend_rid,
           record->source_path
       );
   }
 
   SDL_Log("----------------------------------------------------------");
}