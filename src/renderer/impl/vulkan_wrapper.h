#ifndef RENDERER_VULKAN_WRAPPER_H
#define RENDERER_VULKAN_WRAPPER_H

#include "volk.h"
#include "vk_mem_alloc.h"

const char* vklayer_result_to_string(VkResult result);
void vklayer_print_queueflagbits(VkQueueFlagBits flags);
void vklayer_print_memoryheapflagbits(VkMemoryHeapFlags flags);
void vklayer_print_memorypropertyflagbits(VkMemoryPropertyFlags flags);

#define VK_CHECK(x)                                                         \
do                                                                          \
{                                                                           \
    VkResult err = (x);                                                     \
    if (err != VK_SUCCESS)                                                  \
    {                                                                       \
        fprintf(stderr, "[%s:%d] Vulkan error: %s (%d)\n",                  \
            __FILE__, __LINE__, vklayer_result_to_string(err), (int)(err)); \
        SDL_assert(0 && "VK_CHECK() not successful.");                          \
        abort();                                                            \
    }                                                                       \
} while (0)

#endif  // RENDERER_VULKAN_WRAPPER_H
