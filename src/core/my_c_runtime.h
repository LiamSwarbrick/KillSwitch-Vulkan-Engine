#ifndef MY_C_RUNTIME_H
#define MY_C_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <math.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;
typedef int64_t  s64;
typedef int32_t b32;

typedef uint8_t u8;
typedef int8_t b8;

// NOTE: Old: use SDL_Log and friends instead
// #define ENABLE_VERBOSE_LOGGING
// #ifdef ENABLE_VERBOSE_LOGGING
// #define VERBOSE_LOG(...) printf(__VA_ARGS__)
// #else
// #define VERBOSE_LOG(...) do {} while (0)
// #endif


#define ANSI_CYAN "\x1b[36m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_RESET "\x1b[0m"


//
// Memory Tracker API (like a validation layer around calloc/free during debug mode)
//          (Best simple tool thing I've ever made it's EPIC)
//
// NOTE: Wrapping around calloc to track memory allocations/leaks
// Only tracks in debug mode. (i.e. when NDEBUG is NOT defined)
// So release mode has the fast path: calls calloc/free and does nothing else.
//


// Each allocation gets this struct secretly prepended as a head.
// The user of L_calloc/L_free doesn't see this however, they get
// the data pointer after, aka: ptr + sizeof(ThreadAllocInfo)
typedef struct ThreadAllocInfo
{
    u32 magic_number;  // The magic number doesn't take up more memory because of alignment rules if we didn't have it
    // Why magic number: so I can catch reallocs and frees with invalid old pointers

    char filename[32];
    char funcname[32];
    u32 line_number;
    u64 buffer_size;  // The requested alloc size (not including the prepended header)
    ThreadAllocInfo * prev, * next;
}
ThreadAllocInfo;
#define THREAD_ALLOC_INFO_MAGIC_NUMBER 0xDEADFACE  // Hell yeah!

// Guaruntees of memory alignment:
//
// Verify size of allocation header to ensure padding guaruntees of buffer return by L_malloc/L_calloc
// Because we alloc a header + requested_size then return just the requested size part (which needs to be aligned properly).
//
static_assert(
    sizeof(ThreadAllocInfo) % 8 == 0,
    "ThreadAllocInfo must be paddded to an 8 byte boundary"
);


// --- Tracker for memory leaks ---
// This struct is passed to every allocation and is how we can keep
// track of all the allocations via a linked list.
// A program can have one or many of these depending on how you want
// to group allocations and memory leak checks.
// But allocations are not thread-safe so at least one tracker per thread.
typedef struct ThreadAllocTracker
{
    char tracker_name[32];

    u64 total_allocs;
    u64 total_frees;
    u64 total_bytes_allocated;
    u64 total_bytes_freed;

    ThreadAllocInfo* head;
    ThreadAllocInfo* tail;
}
ThreadAllocTracker;

// Init a tracker
ThreadAllocTracker init_per_thread_allocation_tracker(const char* tracker_name);

// At a point in the code you think all allocations from a tracker should
// have been freed. You can verify whether that is true by calling this functino
// to check for unfreed memory, and it will list each allocation.
// including the allocation's __FILE__, __LINE__ and __func__ for easy debugging.
void check_tracker_for_memory_leaks(ThreadAllocTracker* tracker);


// NOTE: Only implementing calloc to make it simpler (hence less error prone)
// In order to get debugging information for where each memory leak was allocated,
// the L_calloc_implementation call attaches the file name, function name and line number.
// We hide these arguments with by calling the macro L_calloc() instead.
void* L_calloc_implementation(size_t nmemb, size_t size, ThreadAllocTracker* alloc_tracker, const char* const file, const char* const func, int line);
#define L_calloc(nmemb, size, per_thread_alloctracker_pointer) L_calloc_implementation(nmemb, size, per_thread_alloctracker_pointer, __FILE__, __func__, __LINE__)
void* L_realloc(void* ptr, size_t size, ThreadAllocTracker* alloc_tracker);
void L_free(void* ptr, ThreadAllocTracker* alloc_tracker);

// NOTE: May be useful at some point:
// #define L_is_pointer_aligned(ptr, alignment) ((uintptr_t)ptr % alignment == 0)

//
// File API: Probably replace this with SDL file API when finally switching to SDL for actual engine.
//

// Load's entire binary files contents, returns buffer pointer if success,
// and returns NULL on failure to open and read the entire file.
// Returned buffer's alignment is same as malloc i.e. sizeof(void*) (=8 byte boundary).
// The buffer size gets stored in *out_size.
void* L_load_binary_file(const char* const file_path, u64* out_size, ThreadAllocTracker* alloc_tracker);


#ifdef __cplusplus
}
#endif

#endif  // MY_C_RUNTIME_H
