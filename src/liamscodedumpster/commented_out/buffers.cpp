#include "internal_state.h"

#include <bit>  // compute_num_mip_levels() uses std::countl_zero()
// NOTE: If porting to C, C23 has stdc_leading_zeros() in <stdbit.h> header

GPU_Buffer create_buffer(VmaAllocator vma_allocator, u64 size, VkBufferUsageFlags buffer_usage_flags, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage)
{
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;  // The flags are related sparse buffer things and buffer device address things
    buffer_create_info.size = size;
    buffer_create_info.usage = buffer_usage_flags;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.flags = allocation_flags;
    alloc_create_info.usage = memory_usage;
    
    GPU_Buffer gpu_buffer = {};
    VK_CHECK(vmaCreateBuffer(vma_allocator, &buffer_create_info, &alloc_create_info, &gpu_buffer.buffer, &gpu_buffer.allocation, &gpu_buffer.info));

    return gpu_buffer;
}

void destroy_buffer(VmaAllocator vma_allocator, const GPU_Buffer* gpu_buffer)
{
    vmaDestroyBuffer(vma_allocator, gpu_buffer->buffer, gpu_buffer->allocation);
}

GPU_Buffer create_staging_buffer_from_data(VmaAllocator vma_allocator, u8* data, u64 size)
{
    // Staging buffer is specified with transfer src bit because we will copy from it to buffers that lie in video memory.
    GPU_Buffer staging_buffer = create_buffer(vma_allocator,
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO
    );

    // Map staging buffer, memcpy data over, unmap.
    void* staging_pointer = NULL;
    VK_CHECK(vmaMapMemory(vma_allocator, staging_buffer.allocation, &staging_pointer));
    memcpy(staging_pointer, data, size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    return staging_buffer;
}

// create_image_texture2d() is currently in due_rework/
// because it uses the one time submit command buffer.

void destroy_image(VkDevice device, VmaAllocator vma_allocator, GPU_Image gpu_image)
{
    vkDestroyImageView(device, gpu_image.image_view, NULL);
    vmaDestroyImage(vma_allocator, gpu_image.image, gpu_image.allocation);
}

u32 compute_num_mip_levels(u32 image_level0_width, u32 image_level0_height)
{
    // Counting number of mipmaps an image needs
    //
    // Let N := Num mip levels.
    // Let L := max(width, height) of base image (level 0).
    // Each mip level is half resolution of previous one, e.g.:
    // - Mip level 0: length = L    = L / (2^0)
    // - Mip level 1: length = L/2  = L / (2^1)
    // - Mip level 2: length = L/4  = L / (2^2)
    // - Mip level 3: length = L/8  = L / (2^3)
    //
    // Highest mip level has length of 1 pixel so num mip levels N is:
    //    N = 1 + floor( log2( max(width, height) ) )
    // => N = 1 + position of most significant bit set in max(width, height)
    // For 32 bit integers:
    // => N = 1 + (32 - (num leading zeroes + 1))
    // => N = 32 - num leading zeroes

    const u32 bits = image_level0_width | image_level0_height;
    const u32  leading_zeros = std::countl_zero(bits);  // C++
    // const u32  leading_zeros = stdc_leading_zeros(bits);  // C23
    return 32 - leading_zeros;
}

// For Images used for render attachments in a graphics pipeline/renderpass:
GPU_Image create_attachment_image(RenderState* renderstate, VkExtent3D extent, VkFormat format,
    VkImageUsageFlags usage, VkImageAspectFlags aspect_flags, b32 has_msaa)
{
    GPU_Image gpu_image = {
        .current_layout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .image           = VK_NULL_HANDLE,
        .image_view      = VK_NULL_HANDLE,
        .allocation      = VK_NULL_HANDLE,
        .image_extent    = extent,
        .image_format    = format
    };

    // Multisampling
    VkSampleCountFlagBits samples;
    if (has_msaa)
    {
        // TODO: add msaa when asked for it
        SDL_Log("Not implemented MSAA attachment images yet.\n");
        SDL_assert(0 && "Not implemented MSAA attachment images yet");
        abort();
    }
    else
    {
        samples = VK_SAMPLE_COUNT_1_BIT;
    }

    // Allocate and create image
    VkImageCreateInfo image_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .imageType              = VK_IMAGE_TYPE_2D,
        .format                 = format,
        .extent                 = extent,
        .mipLevels              = 1,
        .arrayLayers            = 1,
        .samples                = samples,
        .tiling                 = VK_IMAGE_TILING_OPTIMAL,
        .usage                  = usage,
        .sharingMode            = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount  = 0,
        .pQueueFamilyIndices    = NULL,
        .initialLayout          = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.flags = 0;
    alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateImage(renderstate->vma_allocator, &image_create_info, &alloc_create_info, &gpu_image.image, &gpu_image.allocation, NULL));


    // Create image view
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = gpu_image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VK_CHECK(vkCreateImageView(renderstate->device, &image_view_create_info, NULL, &gpu_image.image_view));
    
    return gpu_image;
}
