#include "mapped_linear_allocator.h"
#include "internal_state.h"

uint64_t PaddedSizeForMappedArena(uint64_t size)
{
    return (size + (MAPPED_ARENA_ALIGNMENT - 1)) & ~(MAPPED_ARENA_ALIGNMENT - 1);
}


MappedArena MakeArenaOnBufferResource(uint32_t underlying_mapped_buffer_rid)
{
    // A series of assertions to ensure the underlying resource is valid to be used as a linear allocator for transient memory.
    FG_Resource* res = &renderstate.registry.resources[underlying_mapped_buffer_rid];
    SDL_assert(res->type = FG_RESOURCE_TYPE_BUFFER);
    SDL_assert((res->buffer.mapped_data != NULL) &&
        "The linear allocator's purpose is efficient upload of CPU to GPU. So only supporting mapped buffers"
    );
    SDL_assert((res->flags & FG_RESOURCE_FLAGS_ON_STARTUP) && 
        "For simplicity, only allow ON_STARTUP resources to become these transient memory arenas."
        "This is because ON_STARTUP resources are the only ones that are guarunteed not to be deleted throughout the program lifetime."
        "So we avoid the situation where a resourceid gets replaced with a new one, thus invalidating the MappedArena."
    );

    // Because of these assertions,
    // we can cache the resource size, buffer device address, and mapped pointer
    // by storing them within the MappedArena struct.
    // Really, the MappedArena struct is just a resource id and the current_offset
    // with current_offset being the number of bytes along the buffer has already been used,
    // so that we know where to put the new data on each push operation.
    return {
        .rid              = underlying_mapped_buffer_rid,
        .gpu_base_address = res->buffer_gpu_address,
        .mapped_data      = (uint8_t*)res->buffer.mapped_data,
        .total_size       = res->buffer.size,
        .current_offset   = 0
    };
}

uint64_t PushToMappedArena(MappedArena* arena, void* data, uint64_t size)
{
    // Align to 64 bytes (helpful/required for many BDA operations)
    uint64_t aligned_offset = PaddedSizeForMappedArena(arena->current_offset);
    SDL_assert((aligned_offset + size <= arena->total_size) &&
        "Size of mapped arena's underlying buffer resource is being exceeded!"
    );

    // Copy data to the mapped pointer
    // NOTE: If we just use memcpy, we'd have to check if the memory was not HOST_COHERENT
    //       and then call vkFlushMappedMemoryRanges(). This VMA helper function does it for us :)
    vmaCopyMemoryToAllocation(renderstate.vma_allocator, data, renderstate.registry.resources[arena->rid].allocation, aligned_offset, size);
    // memcpy(arena->mapped_data + aligned_offset, data, size);
    arena->current_offset = aligned_offset + size;

    
    // Return the absolute GPU address (e.g. for passing to push constants)
    return arena->gpu_base_address + aligned_offset;
}

void ResetMappedArena(MappedArena* arena)
{
    arena->current_offset = 0;
}
