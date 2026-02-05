#include "my_c_runtime.h"

#include <assert.h>

//
// Memory Tracker API
//


ThreadAllocTracker
init_per_thread_allocation_tracker(char const* const tracker_name)
{
    // Init to empty
    ThreadAllocTracker tracker = {};
    strncpy(tracker.tracker_name, tracker_name, sizeof(tracker.tracker_name)-1);
    tracker.tracker_name[sizeof(tracker.tracker_name)-1] = '\0';
    tracker.total_allocs = 0;
    tracker.total_frees = 0;
    tracker.total_bytes_allocated = 0;
    tracker.total_bytes_freed = 0;
    tracker.head = NULL;
    tracker.tail = NULL;

    return tracker;
}

void
check_tracker_for_memory_leaks(ThreadAllocTracker* tracker)
{
    #ifdef NDEBUG
        printf("%s: (Fast path for release mode. Not checking for leaks).\n", tracker->tracker_name);
    #else
        s64 active_allocs = (s64)(tracker->total_allocs) - (s64)(tracker->total_frees);
        s64 active_bytes = (s64)(tracker->total_bytes_allocated) - (s64)(tracker->total_bytes_freed);

        if (active_allocs == 0 && active_bytes == 0)
        {
            printf(ANSI_CYAN "%s:" ANSI_GREEN " No memory leaks found :)" ANSI_RESET " (%" PRIu64 " allocs, %" PRIu64 " frees, %" PRIu64 " total bytes allocated and freed across lifetime).\n", tracker->tracker_name, tracker->total_allocs, tracker->total_frees, tracker->total_bytes_allocated);
        }
        else
        {
            printf(ANSI_MAGENTA "*LEAK* " ANSI_CYAN "%s" ANSI_RESET " detected %" PRIu64 " bytes leaked from %" PRIu64 " active allocs:\n" ANSI_RESET, tracker->tracker_name, active_bytes, active_allocs);

            // Traverse list of allocations and display them
            ThreadAllocInfo* list_entry = tracker->head;
            while (list_entry)
            {
                printf(ANSI_MAGENTA "*----* " ANSI_YELLOW "- %s:%s:line %" PRIu32 ANSI_RESET ": alloc of" ANSI_MAGENTA " %" PRIu64 " bytes" ANSI_RESET " never freed.\n",
                    list_entry->filename, list_entry->funcname, list_entry->line_number, list_entry->buffer_size);
                ThreadAllocInfo* missed_alloc = list_entry;
                list_entry = missed_alloc->next;
                free(missed_alloc);
            }
        }
        
        // Comfirmed empty reset values:
        tracker->total_allocs = 0;
        tracker->total_frees = 0;
        tracker->total_bytes_allocated = 0;
        tracker->total_bytes_freed = 0;
        tracker->head = NULL;
        tracker->tail = NULL;
    #endif
}


static inline ThreadAllocInfo*
get_alloc_info(void* ptr, ThreadAllocTracker* alloc_tracker)
{
    // Get size of original allocation by looking backwards in memory for the prepended header
    ThreadAllocInfo* actual_ptr = (ThreadAllocInfo*)((char*)ptr - sizeof(ThreadAllocInfo));
    if (actual_ptr->magic_number != THREAD_ALLOC_INFO_MAGIC_NUMBER)
    {
        fprintf(stderr, "[ERROR] Invalid ThreadAllocInfo allocation header at %p (got 0x%X)\n", (void*)actual_ptr, actual_ptr->magic_number);
        abort();
    }
    return actual_ptr;
}


void*
L_calloc_implementation(size_t nmemb, size_t size, ThreadAllocTracker* alloc_tracker, const char* const file, const char* const func, int line)
{
    #ifdef NDEBUG
        return calloc(nmemb, size);
    #else
        if (alloc_tracker == NULL)
        {
            fprintf(stderr, "Error (%s): ThreadAllocTracker* was NULL (%s:%s, line %d)\n", __func__, file, func, line);
            exit(1);
        }

        // Allocate buffer with header for the allocation info
        size_t requested_size = nmemb * size;
        ThreadAllocInfo* actual_buffer = (ThreadAllocInfo*)calloc(1, sizeof(ThreadAllocInfo) + requested_size);

        actual_buffer->magic_number = THREAD_ALLOC_INFO_MAGIC_NUMBER;

        strncpy(actual_buffer->filename, file, sizeof(actual_buffer->filename)-1);
        actual_buffer->filename[sizeof(actual_buffer->filename)-1] = '\0';

        strncpy(actual_buffer->funcname, func, sizeof(actual_buffer->funcname)-1);
        actual_buffer->funcname[sizeof(actual_buffer->funcname)-1] = '\0';

        actual_buffer->line_number = line;

        actual_buffer->buffer_size = requested_size;
        actual_buffer->prev = alloc_tracker->tail;
        actual_buffer->next = NULL;
        // INIT_THREAD_ALLOC_INFO(actual_buffer, requested_size, alloc_tracker->tail, file, func, line);

        // Update alloc tracker
        ++alloc_tracker->total_allocs;
        alloc_tracker->total_bytes_allocated += requested_size;

        if (alloc_tracker->tail)
        {
            // Set next of old tail to this
            alloc_tracker->tail->next = actual_buffer;
        }

        // Add to tail of list
        alloc_tracker->tail = actual_buffer;

        // Set head if it's the first element
        if (!alloc_tracker->head)
        {
            alloc_tracker->head = actual_buffer;
        }

        // Return the requested buffer (which is a pointer to the memory after the header/ThreadAllocInfo)
        // NOTE: Alignment guarunteed the same because ThreadAllocInfo is padded to 8 byte boundary
        return (void*)((char*)actual_buffer + sizeof(ThreadAllocInfo));

    #endif
}

void*
L_realloc(void* ptr, size_t size, ThreadAllocTracker* alloc_tracker)
{
    #ifdef NDEBUG
        return realloc(ptr, size);
    #else
        // NOTE:
        // realloc likely does something more efficient under the hood
        // than just allocating again and copying.
        // But because this is debug code, and we don't want multiple
        // allocators (because making sure L_calloc is error free is important
        // and trying to keep multiple error free is more risky),
        // it's efficient enough to just do another L_calloc then memcpy
        // and free the old memory

        if (alloc_tracker == NULL)
        {
            fprintf(stderr, "Error (%s): ThreadAllocTracker* was NULL.\n", __func__);
            exit(1);
        }

        // Following the man page for realloc:
        if (ptr == NULL)
        {
            assert(0 && "ptr == NULL is valid realloc but shitty allocation code, what are you doing. If there is actually a valid reason, then feel free to get rid of this assertion.");
            return L_calloc(1, size, alloc_tracker);
        }
        else if (size == 0)
        {
            assert(0 && "size of 0 is valid realloc but shitty allocation code, what are you doing. If there is actually a valid reason, then feel free to get rid of this assertion.");
            L_free(ptr, alloc_tracker);
            return NULL;
        }

        ThreadAllocInfo* actual_buffer = get_alloc_info(ptr, alloc_tracker);

        // Allocate the new block of the new size
        void* new_memory = L_calloc(1, size, alloc_tracker);

        // Copy the old memory to the new one,
        // making sure not to overrun because the new size may be smaller
        size_t copy_size = actual_buffer->buffer_size;
        if (size < actual_buffer->buffer_size)
        {
            copy_size = size;
        }
        memcpy(new_memory, ptr, copy_size);

        // Free the old memory
        L_free(ptr, alloc_tracker);

        return new_memory;
        
    #endif
}

void
L_free(void* ptr, ThreadAllocTracker* alloc_tracker)
{
    #ifdef NDEBUG
        free(ptr);
    #else
        if (alloc_tracker == NULL)
        {
            fprintf(stderr, "Error (%s): ThreadAllocTracker* was NULL.\n", __func__);
            exit(1);
        }

        ThreadAllocInfo* actual_buffer = get_alloc_info(ptr, alloc_tracker);

        ++alloc_tracker->total_frees;
        alloc_tracker->total_bytes_freed += ((ThreadAllocInfo*)actual_buffer)->buffer_size;

        if (actual_buffer->prev)
        {
            actual_buffer->prev->next = actual_buffer->next;
        }
        else
        {
            // actual_buffer is the head of the list:
            alloc_tracker->head = actual_buffer->next;
        }
        
        if (actual_buffer->next)
        {
            actual_buffer->next->prev = actual_buffer->prev;
        }
        else
        {
            // actual_buffer is the tail of the list:
            alloc_tracker->tail = actual_buffer->prev;
        }

        // Now no reference exists to actual_buffer anymore, so we can delete it.
        free(actual_buffer);
    #endif
}


//
// FILE API
//


void*
L_load_binary_file(const char* const file_path, u64* out_size, ThreadAllocTracker* alloc_tracker)
{
    FILE* file = fopen(file_path, "rb");
    if (file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0)
    {
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_SET);
    u64 file_size = (u64)size;

    // Allocate buffer with file size
    void* buffer = L_calloc(1, file_size, alloc_tracker);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    u64 num_bytes_read = fread(buffer, 1, file_size, file);

    // Make sure the whole file was read
    if (num_bytes_read != file_size)
    {
        printf("Error (%s): Read %" PRIu64 " bytes, excepted %" PRIu64 ".\n", __func__, num_bytes_read, file_size);
        fclose(file);
        L_free(buffer, alloc_tracker);
        return NULL;
    }

    fclose(file);
    *out_size = file_size;
    return buffer;
}

