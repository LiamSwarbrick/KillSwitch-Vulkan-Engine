// NOTE: For A1 don't do it this way
// #define VOLK_IMPLEMENTATION  // <- Only in one translation unit (this translation unit) to avoid conflicts
#include "vk_layer.h"

#include <bit>
// NOTE: If porting to C, C23 has stdc_leading_zeros() in <stdbit.h> header
//       C++ clearly has worse taste in names

#define MY_VK_RESULT_LIST(X) \
    X(VK_SUCCESS) \
    X(VK_NOT_READY) \
    X(VK_TIMEOUT) \
    X(VK_EVENT_SET) \
    X(VK_EVENT_RESET) \
    X(VK_INCOMPLETE) \
    X(VK_ERROR_OUT_OF_HOST_MEMORY) \
    X(VK_ERROR_OUT_OF_DEVICE_MEMORY) \
    X(VK_ERROR_INITIALIZATION_FAILED) \
    X(VK_ERROR_DEVICE_LOST) \
    X(VK_ERROR_MEMORY_MAP_FAILED) \
    X(VK_ERROR_LAYER_NOT_PRESENT) \
    X(VK_ERROR_EXTENSION_NOT_PRESENT) \
    X(VK_ERROR_FEATURE_NOT_PRESENT) \
    X(VK_ERROR_INCOMPATIBLE_DRIVER) \
    X(VK_ERROR_TOO_MANY_OBJECTS) \
    X(VK_ERROR_FORMAT_NOT_SUPPORTED) \
    X(VK_ERROR_FRAGMENTED_POOL) \
    X(VK_ERROR_UNKNOWN) \
    X(VK_ERROR_OUT_OF_POOL_MEMORY) \
    X(VK_ERROR_INVALID_EXTERNAL_HANDLE) \
    X(VK_ERROR_FRAGMENTATION) \
    X(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS) \
    X(VK_PIPELINE_COMPILE_REQUIRED) \
    X(VK_ERROR_NOT_PERMITTED) \
    X(VK_ERROR_SURFACE_LOST_KHR) \
    X(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR) \
    X(VK_SUBOPTIMAL_KHR) \
    X(VK_ERROR_OUT_OF_DATE_KHR) \
    X(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR) \
    X(VK_ERROR_INVALID_SHADER_NV) \
    X(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR) \
    X(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT) \
    X(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) \
    X(VK_THREAD_IDLE_KHR) \
    X(VK_THREAD_DONE_KHR) \
    X(VK_OPERATION_DEFERRED_KHR) \
    X(VK_OPERATION_NOT_DEFERRED_KHR) \
    X(VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR) \
    X(VK_ERROR_COMPRESSION_EXHAUSTED_EXT) \
    X(VK_INCOMPATIBLE_SHADER_BINARY_EXT) \
    X(VK_PIPELINE_BINARY_MISSING_KHR) \
    X(VK_ERROR_NOT_ENOUGH_SPACE_KHR) \
    X(VK_ERROR_VALIDATION_FAILED_EXT) \
    X(VK_ERROR_OUT_OF_POOL_MEMORY_KHR) \
    X(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR) \
    X(VK_ERROR_FRAGMENTATION_EXT) \
    X(VK_ERROR_NOT_PERMITTED_EXT) \
    X(VK_ERROR_NOT_PERMITTED_KHR) \
    X(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT) \
    X(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR) \
    X(VK_PIPELINE_COMPILE_REQUIRED_EXT) \
    X(VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT)

#define MY_VK_QUEUE_FLAG_BITS_LIST(X) \
    X(VK_QUEUE_GRAPHICS_BIT) \
    X(VK_QUEUE_COMPUTE_BIT) \
    X(VK_QUEUE_TRANSFER_BIT) \
    X(VK_QUEUE_SPARSE_BINDING_BIT) \
    X(VK_QUEUE_PROTECTED_BIT) \
    X(VK_QUEUE_VIDEO_DECODE_BIT_KHR) \
    X(VK_QUEUE_VIDEO_ENCODE_BIT_KHR) \
    X(VK_QUEUE_OPTICAL_FLOW_BIT_NV) \
    X(VK_QUEUE_DATA_GRAPH_BIT_ARM)

#define MY_VK_MEMORY_HEAP_FLAG_BITS_LIST(X) \
    X(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) \
    X(VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) \
    X(VK_MEMORY_HEAP_TILE_MEMORY_BIT_QCOM) \
    X(VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR)

#define MY_VK_MEMORY_PROPERTY_FLAG_BITS_LIST(X) \
    X(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) \
    X(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) \
    X(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) \
    X(VK_MEMORY_PROPERTY_HOST_CACHED_BIT) \
    X(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) \
    X(VK_MEMORY_PROPERTY_PROTECTED_BIT) \
    X(VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) \
    X(VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) \
    X(VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)

const char*
vklayer_result_to_string(VkResult result)
{
    // Can't use switch statement since some enum values are duplicated for extensions
    #define MYCASE(name) if (result == name) return #name;
    MY_VK_RESULT_LIST(MYCASE)
    #undef MYCASE
    return "Unknown VkResult Value (the list of values was last updated for vulkan 1.4)\n";
}

void
vklayer_print_queueflagbits(VkQueueFlagBits flags)
{
    #define MYCASE(name) if (flags & name) VERBOSE_LOG(#name" ");
    MY_VK_QUEUE_FLAG_BITS_LIST(MYCASE)
    #undef MYCASE
    VERBOSE_LOG("\n");
}

void
vklayer_print_memoryheapflagbits(VkMemoryHeapFlags flags)
{
    #define MYCASE(name) if (flags & name) VERBOSE_LOG(#name" ");
    MY_VK_MEMORY_HEAP_FLAG_BITS_LIST(MYCASE)
    #undef MYCASE
    VERBOSE_LOG("\n");
}

void
vklayer_print_memorypropertyflagbits(VkMemoryPropertyFlags flags)
{
    #define MYCASE(name) if (flags & name) VERBOSE_LOG(#name" ");
    MY_VK_MEMORY_PROPERTY_FLAG_BITS_LIST(MYCASE)
    #undef MYCASE
    VERBOSE_LOG("\n");
}


////////////////


VkCommandBuffer
vklayer_alloc_cmd_buffer(VkDevice device, VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo cmd_alloc_info = {};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.pNext = NULL;
    cmd_alloc_info.commandPool = command_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &command_buffer));
    
    return command_buffer;
}

VkImageSubresourceRange
vklayer_image_subresource_range(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange subimage = {};
    subimage.aspectMask = aspect_mask;
    subimage.baseMipLevel = 0;
    subimage.levelCount = VK_REMAINING_MIP_LEVELS;
    subimage.baseArrayLayer = 0;
    subimage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subimage;
}

VkImageMemoryBarrier2
vklayer_specify_image_transition_barrier(
    VkImage image,
    VkImageSubresourceRange subimage,
    // VkImageAspectFlags image_aspects_being_transitioned,

    // Before transition:
    VkPipelineStageFlags2 current_pipeline_stage,
    VkAccessFlags2        current_access_flags,
    VkImageLayout         current_layout,
    u32                   current_queue_family_index,

    // After transition:
    VkPipelineStageFlags2 new_pipeline_stage,
    VkAccessFlags2        new_access_flags,
    VkImageLayout         new_layout,
    u32                   new_queue_family_index

    // NOTE: Can default queue family indices to VK_QUEUE_FAMILY_IGNORED when not transfering between queues
)
{
    assert(image != VK_NULL_HANDLE);

    VkImageMemoryBarrier2 image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_barrier.pNext = NULL;

    // Specify what type of shader stages and memory accesses will wait for this transition.
    image_barrier.srcStageMask  = current_pipeline_stage;
    image_barrier.srcAccessMask = current_access_flags;
    image_barrier.dstStageMask  = new_pipeline_stage;
    image_barrier.dstAccessMask = new_access_flags;

    image_barrier.oldLayout = current_layout;
    image_barrier.newLayout = new_layout;

    // NOTE: We are not doing any ownership transfer of images between queues so keep these default:
    image_barrier.srcQueueFamilyIndex = current_queue_family_index;
    image_barrier.dstQueueFamilyIndex = new_queue_family_index;

    // // Specify which aspect of the image is changing e.g. color vs depth information
    // VkImageAspectFlags aspect_mask = image_aspects_being_transitioned;  // (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    // VkImageSubresourceRange subimage = vklayer_image_subresource_range(aspect_mask);

    image_barrier.image = image;
    image_barrier.subresourceRange = subimage;

    return image_barrier;
}

// NOTE: It is more efficient to allow multiple transitions at once instead of doing two sequential vkCmdPipelineBarrier's
//       so I'm implementing it like this instead of a transition single image function.
void
vklayer_cmd_transition_images(VkCommandBuffer cmd, u32 image_barrier_count, const VkImageMemoryBarrier2* image_barriers)
{
    assert(cmd != VK_NULL_HANDLE);
    assert(image_barriers);

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = NULL;

    // Allow the dependency to be satisfied per region rather than requiring the entire framebuffer to be complete.
    // E.g. so tiled-based GPUs can start processing the next subpass as soon as the same tile in the previous subpass is transitioned.
    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependency_info.imageMemoryBarrierCount = image_barrier_count;
    dependency_info.pImageMemoryBarriers = image_barriers;  // Specifies the old and new layouts

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkBufferMemoryBarrier2
vklayer_specify_buffer_barrier(
    VkBuffer buffer, VkDeviceSize buffer_size, VkDeviceSize buffer_offset,
    
    // Before barrirer
    VkPipelineStageFlags2 current_pipeline_stage,
    VkAccessFlags2        current_access_flags,
    u32                   current_queue_family_index,

    // After barrier:
    VkPipelineStageFlags2 new_pipeline_stage,
    VkAccessFlags2        new_access_flags,
    u32                   new_queue_family_index
)
{
    assert(buffer != VK_NULL_HANDLE);

    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.pNext = NULL;

    // Specify what type of shader stages and memory accesses will wait for this barrier
    barrier.srcStageMask = current_pipeline_stage;
    barrier.srcAccessMask = current_access_flags;
    barrier.dstStageMask = new_pipeline_stage;
    barrier.dstAccessMask = new_access_flags;
    
    barrier.srcQueueFamilyIndex = current_queue_family_index;
    barrier.dstQueueFamilyIndex = new_queue_family_index;

    barrier.buffer = buffer;
    barrier.offset = buffer_offset;
    barrier.size   = buffer_size;

    return barrier;
}

void
vklayer_cmd_pipeline_barrier_for_buffers(VkCommandBuffer cmd, u32 barrier_count, const VkBufferMemoryBarrier2* barriers)
{
    // NOTE: In cases where a pipeline barrier that transitions both barriers and images at once is needed.
    // Best to just specify the VkDependencyInfo manually instead of using the helper functions.
    // Because there is less overhead in using a single pipeline barrier (not sure how much overhead).

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = NULL;

    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependency_info.bufferMemoryBarrierCount = barrier_count;
    dependency_info.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

void
vklayer_cmd_blit_image_to_image(VkCommandBuffer cmd,
    VkImage src, VkImage dest, VkExtent2D src_size, VkExtent2D dst_size)
{
    // NOTE: We do this via blitting instead of copying
    //   which lets the source and destination be different resolutions.
    //   However image CopyImage is faster than BlitImage.

    VkImageBlit2 image_blit_region = {};
    image_blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    image_blit_region.pNext = NULL;
    
    // Copy the colour information of the source image to the destination image
    image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.srcSubresource.baseArrayLayer = 0;
    image_blit_region.srcSubresource.layerCount = 1;
    image_blit_region.srcSubresource.mipLevel = 0;

    image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.dstSubresource.baseArrayLayer = 0;
    image_blit_region.dstSubresource.layerCount = 1;
    image_blit_region.dstSubresource.mipLevel = 0;

    // Copy region: from entire source image
    image_blit_region.srcOffsets[0].x = 0;
    image_blit_region.srcOffsets[0].y = 0;
    image_blit_region.srcOffsets[0].z = 0;

    image_blit_region.srcOffsets[1].x = src_size.width;
    image_blit_region.srcOffsets[1].y = src_size.height;
    image_blit_region.srcOffsets[1].z = 1;

    // ...to entire destination
    image_blit_region.dstOffsets[0].x = 0;
    image_blit_region.dstOffsets[0].y = 0;
    image_blit_region.dstOffsets[0].z = 0;

    image_blit_region.dstOffsets[1].x = dst_size.width;
    image_blit_region.dstOffsets[1].y = dst_size.height;
    image_blit_region.dstOffsets[1].z = 1;

    // Create the blit command with linear filtering and optimal image layouts
    VkBlitImageInfo2 blit_image_info2 = {};
    blit_image_info2.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blit_image_info2.pNext = NULL;
    
    blit_image_info2.srcImage = src;
    blit_image_info2.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_image_info2.dstImage = dest;
    blit_image_info2.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit_image_info2.regionCount = 1;
    blit_image_info2.pRegions = &image_blit_region;
    blit_image_info2.filter = VK_FILTER_LINEAR;

    vkCmdBlitImage2(cmd, &blit_image_info2);
}

VkSemaphoreSubmitInfo
vklayer_semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore)
{
    VkSemaphoreSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.semaphore = semaphore;
    submit_info.stageMask = stage_mask;
    submit_info.deviceIndex = 0;
    submit_info.value = 1;

    return submit_info;
}

VkCommandBufferSubmitInfo
vklayer_command_buffer_submit_info(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.commandBuffer = cmd;
    submit_info.deviceMask = 0;

    return submit_info;
}

VkSubmitInfo2
vklayer_submit_info(VkCommandBufferSubmitInfo* cmd_submit_info,
    VkSemaphoreSubmitInfo* signal_semaphore_submit_info, VkSemaphoreSubmitInfo* wait_semaphore_submit_info)
{
    VkSubmitInfo2 submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.pNext = NULL;

    submit_info.waitSemaphoreInfoCount = wait_semaphore_submit_info == NULL ? 0 : 1;
    submit_info.pWaitSemaphoreInfos = wait_semaphore_submit_info;

    submit_info.signalSemaphoreInfoCount = signal_semaphore_submit_info == NULL ? 0 : 1;
    submit_info.pSignalSemaphoreInfos = signal_semaphore_submit_info;

    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = cmd_submit_info;

    return submit_info;
}

VkFormat
vklayer_find_supported_depth_format(VkPhysicalDevice physical_device)
{
    // TODO: Currently just checking for support of D32_SFLOAT, support more formats

    // Preferred formats in order of quality/support
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,           // 32-bit floating point depth (best precision)
        // VK_FORMAT_D32_SFLOAT_S8_UINT,   // 32-bit depth + 8-bit stencil
        // VK_FORMAT_D24_UNORM_S8_UINT,    // 24-bit normalized depth + 8-bit stencil (most common)
        // VK_FORMAT_D16_UNORM             // 16-bit depth (least precision, fastest)
    };

    int num_candidates = sizeof(candidates) / sizeof(VkFormat);
    for (int i = 0; i < num_candidates; ++i)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(physical_device, candidates[i], &format_properties);

        // Check if the format can be used as a depth/stencil attachment
        if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return candidates[i];
        }
    }

    assert(0 && "TODO: Support more than just D32_SFLOAT depth buffer format");
    return VK_FORMAT_UNDEFINED;
}

// TODO: Deprecate this function (Reason: No point creating simple wrappers around vulkan structs, limiting flexability)
VkImageCreateInfo
vklayer_basic_image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent)
{
    VkImageCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;

    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.format = format;
    create_info.extent = extent;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;

    // No MSAA:
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;

    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = usage_flags;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return create_info;
}

// TODO: Deprecate this function (Reason: No point creating simple wrappers around vulkan structs, limiting flexability)
VkImageViewCreateInfo
vklayer_basic_imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;

    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = aspect_flags;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    return create_info;
}

u32
compute_num_mip_levels(u32 image_level0_width, u32 image_level0_height)
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
