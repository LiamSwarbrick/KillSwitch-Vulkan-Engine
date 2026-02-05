#include "internal_state.h"

#include "SDL3/SDL_vulkan.h"

RenderState renderstate;

// NOTE(Liam): The only mutable internal state for renderer is this renderstate.
// All other global state here should be const

#define API_VERSION VK_API_VERSION_1_4
const char* app_name = "gotonwrongtrainfuck";
const u32 app_version = VK_MAKE_VERSION(0, 0, 0);
const char* engine_name = "advengine_renderer_in_progress";
const u32 engine_version = VK_MAKE_VERSION(0, 0, 0);


// Validation layers:
#ifdef NDEBUG
const b32 request_validation_layers = 0;
const char* const* validation_layers = NULL;
const u32 validation_layers_count = 0;
#else
const b32 request_validation_layers = 1;
const char* const validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};
const u32 validation_layers_count = sizeof(validation_layers) / sizeof(char*);
#endif


// Vulkan Instance Extensions
#ifdef NDEBUG
const char* const extra_instance_extensions[] = { "" };
const u32 extra_instance_extensions_count = 0;
#else
const char* const extra_instance_extensions[] = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
const u32 extra_instance_extensions_count = sizeof(extra_instance_extensions) / sizeof(char*);
#endif


// Vulkan Device Extensions
const char* const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
const u32 device_extensions_count = sizeof(device_extensions) / sizeof(char*);


static VKAPI_ATTR VkBool32 VKAPI_CALL
InternalVulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        // SDL_LogError(SDL_LOG_CATEGORY_ERROR, ANSI_CYAN "Validation Layer Verbose: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        // SDL_LogError(SDL_LOG_CATEGORY_ERROR, ANSI_CYAN "Validation Layer Info: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;
        
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:

        SDL_LogError(SDL_LOG_CATEGORY_ERROR, ANSI_CYAN "Validation Layer %s: " ANSI_YELLOW "%s\n" ANSI_RESET,
            messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "Warning" : "Error",
            pCallbackData->pMessage);
        // *(volatile int*)0 = 0;  // Crash the program so I can backtrace
        renderstate.program_caused_vulkan_validation_layer_errors = 1;
        break;
        default: break;
    }

    return VK_FALSE;
}


bool Renderer_Init(const Renderer_InitInfo* info)
{
    // TODO: Initialize vulkan, storing the stuff from my old renderer into internal render state

    // Init the main memory tracker for the main thread which in debug mode we can query it for memory leaks during cleanup
    renderstate.main.tt = init_per_thread_allocation_tracker("Renderer_MainThreadTracker");

    // Init from Renderer_InitInfo
    renderstate.window = info->window;
    renderstate.using_validation_layers = info->enable_validation;

    // Load a few Vulkan procs required to make a VkInstance
    VK_CHECK(volkInitialize());
    if (volkGetInstanceVersion() < API_VERSION)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Sorry, Vulkan %d.%d is strictly required.\n", VK_API_VERSION_MAJOR(API_VERSION), VK_API_VERSION_MINOR(API_VERSION));
        return false;
    }

    // Display Vulkan loader version
    u32 loader_api_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion)
    {
        // This proc was added in Vulkan 1.1
        VK_CHECK(vkEnumerateInstanceVersion(&loader_api_version));
    }
    SDL_Log("Vulkan loader version: %d.%d.%d (variant %d)\n",
        VK_API_VERSION_MAJOR(loader_api_version), VK_API_VERSION_MINOR(loader_api_version),
        VK_API_VERSION_PATCH(loader_api_version), VK_API_VERSION_VARIANT(loader_api_version)
    );

    // Enable validation layers
    if (renderstate.using_validation_layers)
    {
        u32 available_layer_count;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));
        
        VkLayerProperties* available_layers = (VkLayerProperties*)L_calloc(available_layer_count, sizeof(VkLayerProperties), &renderstate.main.tt);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));
        
        // For each layer in validation_layers, make sure it's in the available_layers array
        b32 found_all_layers = 1;
        for (u32 i = 0; i < validation_layers_count; ++i)
        {
            b32 requested_layer_available = 0;
            for (u32 j = 0; j < available_layer_count; ++j)
            {
                if (strcmp(validation_layers[i], available_layers[j].layerName) == 0)
                {
                    requested_layer_available = 1;
                }
            }
            
            if (!requested_layer_available)
            {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Validation layer requested but not available: %s\n", validation_layers[i]);
            }

            found_all_layers &= requested_layer_available;
        }
        if (!found_all_layers)
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "All requested validation layers must be available... Exiting");
            return false;
        }

        L_free(available_layers, &renderstate.main.tt);

        SDL_Log("Debug mode: Using validation layers.\n");
    }
    else
    {
        SDL_Log("Release mode: No validation layers.\n");
    }

    // Create instance
    {
        renderstate.instance = VK_NULL_HANDLE;

        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = NULL,
            .pApplicationName = app_name,
            .applicationVersion = app_version,
            .pEngineName = engine_name,
            .engineVersion = engine_version,
            .apiVersion = API_VERSION
        };

        // TODO: If using optional functionality, query available extensions first with vkEnumerateInstanceExtensionProperties()
        //       Similarly, should check if requested extensions are available first.
        // {
        //     u32 extension_count = 0;
        //     vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

            
        // }

        u32 sdl_extensions_count = 0;
        char const* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);

        // Also include the debug utils extensions if using_validation_layers
        u32 instance_extensions_count = sdl_extensions_count + extra_instance_extensions_count + (renderstate.using_validation_layers ? 1 : 0);
        const char** instance_extensions = (const char**)L_calloc(sizeof(char*), instance_extensions_count, &renderstate.main.tt);
        for (u32 i = 0; i < sdl_extensions_count; ++i)
        {
            instance_extensions[i] = sdl_extensions[i];
        }
        for (u32 i = 0; i < extra_instance_extensions_count; ++i)
        {
            instance_extensions[sdl_extensions_count + i] = extra_instance_extensions[i];
        }
        if (renderstate.using_validation_layers)
        {
            instance_extensions[sdl_extensions_count + extra_instance_extensions_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        
        // Display extensions
        SDL_Log("Instance Extensions: ");
        if (instance_extensions_count)
        {
            for (u32 i = 0; i < instance_extensions_count-1; ++i)
                SDL_Log("%s, ", instance_extensions[i]);
            SDL_Log("%s\n", instance_extensions[instance_extensions_count-1]);
        }
        else
        {
            SDL_Log("None\n");
        }

        VkInstanceCreateInfo instance_create_info = {};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &app_info;
        instance_create_info.enabledExtensionCount = instance_extensions_count;
        instance_create_info.ppEnabledExtensionNames = instance_extensions;

        if (renderstate.using_validation_layers)
        {
            instance_create_info.enabledLayerCount = validation_layers_count;
            instance_create_info.ppEnabledLayerNames = validation_layers;
        }
        else
        {
            instance_create_info.enabledLayerCount = 0;
            instance_create_info.ppEnabledLayerNames = NULL;
            instance_create_info.pNext = NULL;
        }


        VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &renderstate.instance));
        L_free(instance_extensions, &renderstate.main.tt);

        SDL_assert(renderstate.instance != VK_NULL_HANDLE);
    }

    // Get the rest of the Vulkan procs that our VkInstance requires using volk 
    volkLoadInstance(renderstate.instance);
    

    // Setup debug message callback
    if (renderstate.using_validation_layers)
    {
        renderstate.debug_messenger = VK_NULL_HANDLE;


        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
        debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_messenger_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        debug_messenger_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        
        debug_messenger_create_info.pfnUserCallback = InternalVulkanDebugCallback;
        debug_messenger_create_info.pUserData = NULL;

        VK_CHECK(vkCreateDebugUtilsMessengerEXT(renderstate.instance, &debug_messenger_create_info, NULL, &renderstate.debug_messenger));


        SDL_assert(renderstate.debug_messenger != VK_NULL_HANDLE);
    }
    else
    {
        renderstate.debug_messenger = VK_NULL_HANDLE;
    }

    // Create a window surface
    {
        renderstate.surface = VK_NULL_HANDLE;

        // NOTE: The VK_KHR_surface extension is part of the list from SDL_Vulkan_GetInstanceExtensions
        // SDL handles the differences in vulkan surface creation between Windows and Linux
        if (!SDL_Vulkan_CreateSurface(renderstate.window, renderstate.instance, NULL, &renderstate.surface))
        {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create SDL Vulkan Window Surface\n");
            return false;
        }

        SDL_Log("Created Window Surface\n");

        SDL_assert(renderstate.surface != VK_NULL_HANDLE);
    }

    // Choose a physical device
    // NOTE: We had to create the window surface directly after instance
    // creation because it influences physical device selection.
    {
        renderstate.physical_device = VK_NULL_HANDLE;

        u32 device_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(renderstate.instance, &device_count, NULL));
        if (device_count == 0)
        {
            fprintf(stderr, "Could not find a GPU with Vulkan support... Exiting");
            return false;
        }

        VkPhysicalDevice* devices = (VkPhysicalDevice*)L_calloc(device_count, sizeof(VkPhysicalDevice), &renderstate.main.tt);
        VK_CHECK(vkEnumeratePhysicalDevices(renderstate.instance, &device_count, devices));

        // Give each device a suitability score and pick the best one (e.g. dGPU > iGPU)
        int* device_suitability_score = (int*)L_calloc(device_count, sizeof(int), &renderstate.main.tt);
        for (u32 i = 0; i < device_count; ++i)
        {
            SDL_Log("Device %d:\n", i);
            device_suitability_score[i] = score_physical_device_and_check_required_features(devices[i]);
        }
        
        // Select device with highest scored suitability
        int candidate_device_index = 0;
        for (u32 i = 0; i < device_count; ++i)
        {
            if (device_suitability_score[i] > device_suitability_score[candidate_device_index])
            {
                candidate_device_index = i;
            }
        }
        if (device_suitability_score[candidate_device_index] < 0)
        {
            fprintf(stderr, "No suitable physical device for this vulkan program :(\n");
            return false;
        }
        renderstate.physical_device = devices[candidate_device_index];
    
        // Store physical device properties
        vkGetPhysicalDeviceProperties(renderstate.physical_device, &renderstate.physical_device_properties);

        SDL_Log("Selected device %d.\n", candidate_device_index);
        SDL_Log("Device Extensions: ");
        if (device_extensions_count)
        {
            for (u32 i = 0; i < device_extensions_count-1; ++i)
                SDL_Log("%s, ", device_extensions[i]);
            SDL_Log("%s\n", device_extensions[device_extensions_count-1]);
        }
        else
        {
            SDL_Log("None\n");
        }

        L_free(device_suitability_score, &renderstate.main.tt);
        L_free(devices, &renderstate.main.tt);

        SDL_assert(renderstate.physical_device != VK_NULL_HANDLE);
    }

    // Find queue families of selected physical device
    {
        renderstate.queue_family_indices = get_physical_device_queue_family_indices(renderstate.physical_device);
        
        SDL_Log("Graphics queue family at index %d\n", renderstate.queue_family_indices.graphics_family);
        SDL_Log("Presentation (surface) queue family at index %d\n", renderstate.queue_family_indices.present_family);


        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            SDL_assert(renderstate.queue_family_indices.array[i] > -1);
        }
    }

    // Create Logical Device
    {
        renderstate.device = VK_NULL_HANDLE;

        // Create the queues we will submit commands to
        VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)L_calloc(NUM_QUEUE_FAMILY_INDICES, sizeof(VkDeviceQueueCreateInfo), &renderstate.main.tt);
        float queue_priorities[] = { 1.0f };

        // Queues can be available for multiple queue families, so we make sure we only make the unique queues
        int num_unique_queues = 0;
        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            // See if this queue is unique so far
            b32 is_unique_queue = 1;
            for (int j = 0; j < i; ++j)
                if (renderstate.queue_family_indices.array[j] == renderstate.queue_family_indices.array[i])
                    is_unique_queue = 0;
            
            if (is_unique_queue)
            {
                VkDeviceQueueCreateInfo queue_create_info = {};
                queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_info.queueFamilyIndex = (u32)renderstate.queue_family_indices.array[i];
                queue_create_info.queueCount = 1;
                queue_create_info.pQueuePriorities = queue_priorities;

                queue_create_infos[num_unique_queues] = queue_create_info;
                ++num_unique_queues;
            }
        }
        
        // The physical device features we are using get specified here
        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;
        device_features.geometryShader = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vk13_features = {};
        vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13_features.dynamicRendering = VK_TRUE;
        vk13_features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceVulkan14Features vk14_features = {};
        vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vk14_features.maintenance5 = VK_TRUE;

        // GPU pointers
        VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features = {};
        buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        buffer_device_address_features.bufferDeviceAddress = VK_TRUE;


        // The main device is created here, with the queues and features we just specified
        VkDeviceCreateInfo device_create_info = {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // pNext chain of features
        device_create_info.pNext = &vk14_features;
        vk14_features.pNext = &vk13_features;
        vk13_features.pNext = &buffer_device_address_features;
        buffer_device_address_features.pNext = NULL;

        device_create_info.queueCreateInfoCount = num_unique_queues;
        device_create_info.pQueueCreateInfos = queue_create_infos;
        device_create_info.enabledExtensionCount = device_extensions_count;
        device_create_info.ppEnabledExtensionNames = device_extensions;
        device_create_info.pEnabledFeatures = &device_features;

        // Older Vulkan APIs had distinction between instance and device extensions.
        // Later versions don't need to specify extensions for the device 
        // but we do it anyway for compatability with older versions
        device_create_info.enabledLayerCount = validation_layers_count;
        device_create_info.ppEnabledLayerNames = validation_layers;
        
        VK_CHECK(vkCreateDevice(renderstate.physical_device, &device_create_info, NULL, &renderstate.device));

        if (!renderstate.using_validation_layers)
        {
            // Volk: Specialize Vulkan Functions for this device (optional)
            volkLoadDevice(renderstate.device);
            // NOTE: This volk functionality is not suitable for applications that want to use multiple VkDevice objects concurrently.
            // But it has a tiny performance improvement by having one less indirection by loading the device's specific functions directly.
        }

        // Get the queue handles from the device
        vkGetDeviceQueue(renderstate.device, renderstate.queue_family_indices.graphics_family, 0, &renderstate.graphics_queue);
        vkGetDeviceQueue(renderstate.device, renderstate.queue_family_indices.present_family, 0, &renderstate.presentation_queue);

        L_free(queue_create_infos, &renderstate.main.tt);

        SDL_Log("Created Logcal Device.\n");

        SDL_assert(renderstate.device != VK_NULL_HANDLE);
    }

    // Init Vulkan Memory Allocator
    {
        renderstate.vma_allocator = VK_NULL_HANDLE;

        VmaAllocatorCreateInfo allocator_create_info = {};
        allocator_create_info.physicalDevice = renderstate.physical_device;
        allocator_create_info.device = renderstate.device;
        allocator_create_info.instance = renderstate.instance;
        allocator_create_info.flags =
            VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT  // For GPU pointers
        ;
  
        VmaVulkanFunctions vulkan_functions;
        VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocator_create_info, &vulkan_functions));
        allocator_create_info.pVulkanFunctions = &vulkan_functions;

        VK_CHECK(vmaCreateAllocator(&allocator_create_info, &renderstate.vma_allocator));

        SDL_Log("Vulkan Memory Allocator Initialised\n");

        SDL_assert(renderstate.vma_allocator != VK_NULL_HANDLE);
    }

    old_stuff_init(&renderstate);

    

    return true;
}

void Renderer_Shutdown()
{
    SDL_Log("******** Renderer_Shutdown() ********\n");

    // Ensure device is not doing anywork before destroying it's stuff.
    vkDeviceWaitIdle(renderstate.device);

    old_stuff_clean(&renderstate);


    // Destroy fundamental Vulkan objects
    vmaDestroyAllocator(renderstate.vma_allocator);
    vkDestroyDevice(renderstate.device, NULL);
    vkDestroySurfaceKHR(renderstate.instance, renderstate.surface, NULL);
    if (renderstate.using_validation_layers)
    {
        vkDestroyDebugUtilsMessengerEXT(renderstate.instance, renderstate.debug_messenger, NULL);
    }
    vkDestroyInstance(renderstate.instance, NULL);

    // Destroy Window
    SDL_DestroyWindowSurface(renderstate.window);
    SDL_DestroyWindow(renderstate.window);

    // Display if we had correct Vulkan API usage
    if (renderstate.using_validation_layers)
    {
        if (!renderstate.program_caused_vulkan_validation_layer_errors)
        {
            // Vulkan clean up successful
            SDL_Log(ANSI_CYAN "Vulkan cleanup was successful: No complaints from the enabled validation layers.\n" ANSI_RESET);
        }
        else
        {
            SDL_Log(ANSI_MAGENTA "Vulkan validation layer errors/warnings appeared :(\n" ANSI_RESET);
        }
    }

    // Report memory leaks of main tracker
    SDL_Log("\n***********  Memory Tracker Results ***********\n");
    check_tracker_for_memory_leaks(&renderstate.main.tt);
}

void Renderer_OnWindowResize()
{

}

/////////////////

// NOTE: Need to free SwapChainSupportDetails.formats and .present_modes after use
SwapChainSupportDetails get_and_alloc_swap_chain_support_details(VkPhysicalDevice physical_device)
{
    SwapChainSupportDetails details = {};

    // Capabilities are based on the VkPhysicalDevice and the VkSurfaceKHR
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, renderstate.surface, &details.capabilities);

    // Query the supported surface formats 
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, renderstate.surface, &details.format_count, NULL);
    if (details.format_count != 0)
    {
        details.formats = (VkSurfaceFormatKHR*)L_calloc(details.format_count, sizeof(VkSurfaceFormatKHR), &renderstate.main.tt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, renderstate.surface, &details.format_count, details.formats);
    }

    // Query the presentation modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, renderstate.surface, &details.present_mode_count, NULL);
    if (details.present_mode_count != 0)
    {
        details.present_modes = (VkPresentModeKHR*)L_calloc(details.present_mode_count, sizeof(VkPresentModeKHR), &renderstate.main.tt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, renderstate.surface, &details.present_mode_count, details.present_modes);
    }

    return details;
}

void free_swap_chain_support_details(SwapChainSupportDetails details)
{
    if (details.formats)       L_free(details.formats, &renderstate.main.tt);
    if (details.present_modes) L_free(details.present_modes, &renderstate.main.tt);
}

QueueFamilyIndices get_physical_device_queue_family_indices(VkPhysicalDevice physical_device)
{
    QueueFamilyIndices queue_family_indices = {};

    // Each queue family index initialized to -1 so we can test for failure
    memset(&queue_family_indices, -1, sizeof(QueueFamilyIndices));

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)L_calloc(queue_family_count, sizeof(VkQueueFamilyProperties), &renderstate.main.tt);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    // Check for support of each required queue for each queue family
    b32* queue_families_support_for_graphics = (b32*)L_calloc(queue_family_count, sizeof(b32), &renderstate.main.tt);
    b32* queue_families_support_for_presentation = (b32*)L_calloc(queue_family_count, sizeof(b32), &renderstate.main.tt);
    // etc.
    for (u32 i = 0; i < queue_family_count; ++i)
    {
        SDL_Log("--- Queue family %d's bits: ", i);
        vklayer_print_queueflagbits((VkQueueFlagBits)queue_families[i].queueFlags);

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queue_families_support_for_graphics[i] = 1;
        }

        VkBool32 presentation_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, renderstate.surface, &presentation_support);
        if (presentation_support)
        {
            queue_families_support_for_presentation[i] = 1;
        }

        // etc.
    }

    // Setting queue family indices:
    // We will prefer that the graphics and presentation family be on the same queue for best performance
    for (u32 i = 0; i < queue_family_count; ++i)
    {
        if (queue_families_support_for_graphics[i] && queue_families_support_for_presentation[i])
        {
            queue_family_indices.graphics_family = i;
            queue_family_indices.present_family = i;
        }
    }
    // Otherwise, just find all the queue types we need
    for (u32 i = 0; i < queue_family_count; ++i)
    {
        if (queue_families_support_for_graphics[i] && queue_family_indices.graphics_family == -1)
        {
            queue_family_indices.graphics_family = i;
        }
        if (queue_families_support_for_presentation[i] && queue_family_indices.present_family == -1)
        {
            queue_family_indices.present_family = i;
        }
        // etc.
    }
    L_free(queue_families_support_for_graphics, &renderstate.main.tt);
    L_free(queue_families_support_for_presentation, &renderstate.main.tt);
    L_free(queue_families, &renderstate.main.tt);

    return queue_family_indices;
}


int score_physical_device_and_check_required_features(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    u32 supported_version_major = VK_API_VERSION_MAJOR(device_properties.apiVersion);
    u32 supported_version_minor = VK_API_VERSION_MINOR(device_properties.apiVersion);
    u32 supported_version_patch = VK_API_VERSION_PATCH(device_properties.apiVersion);

    SDL_Log("--- name: %s\n", device_properties.deviceName);
    SDL_Log("--- supports up to Vulkan %d.%d.%d\n",
            supported_version_major,
            supported_version_minor,
            supported_version_patch
    );

    int suitability_score = 0;
    {
        // For now, just assume we want dGPU if it exists, and don't go too deep into feature checking.
        switch (device_properties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                SDL_Log("--- is dGPU... lovely :)\n");
                suitability_score += 1000;    
                break;

            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                SDL_Log("--- is iGPU... decent :^)\n");
                suitability_score += 500;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                // TODO: vGPU could be better or worse than the iGPU on a machine, need to check more features in the future (good enough for now)
                SDL_Log("--- is vGPU (virtual GPU)... who knows? TODO: Further checks for if it's a good vGPU e.g. cloud dGPU\n");
                suitability_score += 200;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                SDL_Log("--- is CPU... meh :(\n");
                suitability_score += 0;
                break;
            
            default:
                SDL_Log("--- dunno what type of device this is :(\n");
                suitability_score += 0;
        }
        
        // Check memory properties of this physical device
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        // Display memory heaps and types
        SDL_Log("--- Heaps: has %d memory heap.\n", memory_properties.memoryHeapCount);
        for (u32 i = 0; i < memory_properties.memoryHeapCount; ++i)
        {
            SDL_Log("--- - heap %i: %lu MiB, flags: ", i, memory_properties.memoryHeaps[i].size / (1024*1024));
            vklayer_print_memoryheapflagbits(memory_properties.memoryHeaps[i].flags);
        }

        SDL_Log("--- Memory types: %d types\n", memory_properties.memoryTypeCount);
        for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
        {
            SDL_Log("--- - type %d: from heap %d, flags: ", i, memory_properties.memoryTypes[i].heapIndex);
            vklayer_print_memorypropertyflagbits(memory_properties.memoryTypes[i].propertyFlags);
        }
    }

    // Check required features are available
    b32 is_gpu_suitable = 0;
    {
        // Supports our required Vulkan version
        b32 supports_required_vulkan_version = 0;
        if (supported_version_major >= VK_API_VERSION_MAJOR(API_VERSION) ||
            (supported_version_major == VK_API_VERSION_MAJOR(API_VERSION) && supported_version_minor >= VK_API_VERSION_MINOR(API_VERSION)))
        {
            supports_required_vulkan_version = 1;
        }

        // Require some device features using VK_KHR_get_physical_device_properties2 that was introduced in Vulkan 1.1
        b32 all_required_features_supported = 0;
        if (supported_version_major > 1 || (supported_version_major == 1 && supported_version_minor >= 1))
        {
            VkPhysicalDeviceVulkan13Features vk13_features = {};
            vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

            VkPhysicalDeviceVulkan14Features vk14_features = {};
            vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

            VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features = {};
            buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

            VkPhysicalDeviceFeatures2 device_features = {};
            device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

            // pNext chain of features
            device_features.pNext = &vk14_features;
            vk14_features.pNext = &vk13_features;
            vk13_features.pNext = &buffer_device_address_features;
            buffer_device_address_features.pNext = NULL;

            vkGetPhysicalDeviceFeatures2(physical_device, &device_features);
        
            all_required_features_supported = 1;
            SDL_Log("--- Anisotropic filtering: %d\n", device_features.features.samplerAnisotropy);
            all_required_features_supported = all_required_features_supported  && device_features.features.samplerAnisotropy;
            if (device_features.features.samplerAnisotropy)
            {
                printf("--- - Max anisotropy: %f\n", device_properties.limits.maxSamplerAnisotropy);
            }
            
            SDL_Log("--- Synchronization 2: %d\n", vk13_features.synchronization2);
            all_required_features_supported = all_required_features_supported  && vk13_features.synchronization2;
            SDL_Log("--- Dynamic Rendering: %d\n", vk13_features.dynamicRendering);
            all_required_features_supported = all_required_features_supported  && vk13_features.dynamicRendering;
            
            // Maintenance5 means we don't need VkShaderModule.
            SDL_Log("--- Maintenance 5: %d\n", vk14_features.maintenance5);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance5;
            SDL_Log("--- Maintenance 6: %d\n", vk14_features.maintenance6);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance6;

            // Buffer device address for GPU pointers
            SDL_Log("--- Buffer Device Address: %d (not using it at the moment though)\n", buffer_device_address_features.bufferDeviceAddress);
            all_required_features_supported = all_required_features_supported  && buffer_device_address_features.bufferDeviceAddress;

            // Geometry shaders
            SDL_Log("--- Geometry shaders: %d\n", device_features.features.geometryShader);
            all_required_features_supported = all_required_features_supported && device_features.features.geometryShader;
        }

        // Requires the necessary queue families
        QueueFamilyIndices indices = get_physical_device_queue_family_indices(physical_device);
        b32 all_required_queues_supported = 
                indices.graphics_family != -1 &&
                indices.present_family != -1;

        // Requires some device extensions
        b32 all_required_extensions_available = 0;
        if (all_required_queues_supported)
        {
            u32 available_extensions_count;
            vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_count, NULL);
            VkExtensionProperties* available_extensions = (VkExtensionProperties*)L_calloc(available_extensions_count, sizeof(VkExtensionProperties), &renderstate.main.tt);
            vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_count, available_extensions);

            // Check whether each required extension (device_extensions array) is in the available_extensions array
            all_required_extensions_available = 1;
            for (u32 required_i = 0; required_i < device_extensions_count; ++required_i)
            {
                b32 required_extension_is_available = 0;
                for (u32 available_i = 0; available_i < available_extensions_count; ++available_i)
                {
                    if (strcmp(device_extensions[required_i], available_extensions[available_i].extensionName) == 0)
                    {
                        required_extension_is_available = 1;
                    }
                }

                if (required_extension_is_available == 0)
                {
                    all_required_extensions_available = 0;
                    break;
                }
            }

            L_free(available_extensions, &renderstate.main.tt);
        }

        // Swapchain support details
        b32 is_swapchain_adequate = 0;
        if (all_required_extensions_available)  // Must only query swapchain support after making sure the previous extensions to do so were available
        {
            SwapChainSupportDetails details = get_and_alloc_swap_chain_support_details(physical_device);
            is_swapchain_adequate = details.format_count > 0 && details.present_mode_count > 0;
            free_swap_chain_support_details(details);
        }

        SDL_Log("--- Checking against requirements: VersionUpToDate:%d, Features:%d, Queues:%d, Extensions:%d, SwapChain:%d\n",
                supports_required_vulkan_version, all_required_features_supported, all_required_queues_supported, all_required_extensions_available, is_swapchain_adequate
        );
        is_gpu_suitable =
            supports_required_vulkan_version &&
            all_required_features_supported &&
            all_required_queues_supported &&
            all_required_extensions_available &&
            is_swapchain_adequate;
    }
    
    if (is_gpu_suitable)
    {
        return suitability_score;
    }
    else
    {
        return -1;
    }
}
