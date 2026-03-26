#ifndef RENDERER_MAPPED_LINEAR_ALLOCATOR_H
#define RENDERER_MAPPED_LINEAR_ALLOCATOR_H

#include <stdint.h>

typedef struct MappedArena
{
    uint32_t rid;               // ON_STARTUP flagged resources only for simplicity.
    uint64_t gpu_base_address;  // Cached info about the resouce
    uint8_t* mapped_data;       // ..^..
    uint32_t total_size;        // ..^..

    uint32_t current_offset;    // State of the linear allocator
}
MappedArena;

/*
Use an underlying mapped buffer resource as a linear allocator.
The first use case is objects buffer, each frame, all the object transforms
get written to a mapped buffer via this arena.
*/
MappedArena MakeArenaOnBufferResource(uint32_t underlying_mapped_buffer_rid);

/*
Push data to buffer at base_address + current_offset (with alignment of 64 bytes).
Return the buffer device address where this data now resides (AKA the absolute GPU address).

Aligns data in buffer to 64 bytes for a number of reasons.
For instance, each object we push will never be part of multiple cache lines if less than 64 bytes.
Also important for coalesced memory accesses both with the Write-Combining Buffers on the CPU
and so the warp threads can combine their memory fetches.

FUTURE: If a tightly packed array is necessary, then add another function
that allows a custom alignment (and I guess, set this alignment in the layout qualifier
buffer_reference_align in GLSL).
*/
uint64_t PushToMappedArena(MappedArena* arena, void* data, uint64_t size);

/*
All this does it set current_offset back to 0
so that we can write to the beggining of the buffer again.
*/
void ResetMappedArena(MappedArena* arena);

#endif  // RENDERER_MAPPED_LINEAR_ALLOCATOR_H
