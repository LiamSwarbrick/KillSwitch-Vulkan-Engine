#include "internal_state.h"
#include "pass_definitions.h"

#include "SDL3/SDL_vulkan.h"

RenderState renderstate;

// NOTE(Liam): The only mutable internal state for renderer is this renderstate.
// All other global state here should be const

#define API_VERSION VK_API_VERSION_1_4
const char* app_name = "quantocostoilengino";
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
        // fprintf(stderr, ANSI_CYAN "Validation Layer Verbose: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        // fprintf(stderr, ANSI_CYAN "Validation Layer Info: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;
        
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:

        fprintf(stderr, ANSI_CYAN "Validation Layer %s: " ANSI_YELLOW "%s\n" ANSI_RESET,
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

    // Init internal state from Renderer_InitInfo
    renderstate.window = info->window;
    renderstate.using_validation_layers = info->enable_validation;

    // Load a few Vulkan procs required to make a VkInstance
    VK_CHECK(volkInitialize());
    if (volkGetInstanceVersion() < API_VERSION)
    {
        fprintf(stderr, "Sorry, Vulkan %d.%d is strictly required.\n", VK_API_VERSION_MAJOR(API_VERSION), VK_API_VERSION_MINOR(API_VERSION));
        return false;
    }

    // Display Vulkan loader version
    u32 loader_api_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion)
    {
        // This proc was added in Vulkan 1.1
        VK_CHECK(vkEnumerateInstanceVersion(&loader_api_version));
    }
    printf("Vulkan loader version: %d.%d.%d (variant %d)\n",
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
                fprintf(stderr, "Validation layer requested but not available: %s\n", validation_layers[i]);
            }

            found_all_layers &= requested_layer_available;
        }
        if (!found_all_layers)
        {
            fprintf(stderr, "All requested validation layers must be available... Exiting");
            return false;
        }

        L_free(available_layers, &renderstate.main.tt);

        printf("Debug mode: Using validation layers.\n");
    }
    else
    {
        printf("Release mode: No validation layers.\n");
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
        printf("Instance Extensions: ");
        if (instance_extensions_count)
        {
            for (u32 i = 0; i < instance_extensions_count-1; ++i)
                printf("%s, ", instance_extensions[i]);
            printf("%s\n", instance_extensions[instance_extensions_count-1]);
        }
        else
        {
            printf("None\n");
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
            fprintf(stderr, "Failed to create SDL Vulkan Window Surface\n");
            return false;
        }

        printf("Created Window Surface\n");

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
        printf("\n");
        for (u32 i = 0; i < device_count; ++i)
        {
            printf("Device %d:\n", i);
            device_suitability_score[i] = score_physical_device_and_check_required_features(devices[i]);
            printf("\n");
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

        printf("Selected device %d.\n", candidate_device_index);
        printf("Device Extensions: ");
        if (device_extensions_count)
        {
            for (u32 i = 0; i < device_extensions_count-1; ++i)
                printf("%s, ", device_extensions[i]);
            printf("%s\n", device_extensions[device_extensions_count-1]);
        }
        else
        {
            printf("None\n");
        }

        L_free(device_suitability_score, &renderstate.main.tt);
        L_free(devices, &renderstate.main.tt);

        SDL_assert(renderstate.physical_device != VK_NULL_HANDLE);
    }

    // Find queue families of selected physical device
    {
        renderstate.queue_family_indices = get_physical_device_queue_family_indices(renderstate.physical_device);
        
        printf("Graphics queue family at index %d\n", renderstate.queue_family_indices.graphics_family);
        printf("Presentation (surface) queue family at index %d\n", renderstate.queue_family_indices.present_family);


        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            SDL_assert(renderstate.queue_family_indices.array[i] != UINT32_MAX);
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
                queue_create_info.queueFamilyIndex = renderstate.queue_family_indices.array[i];
                queue_create_info.queueCount = 1;
                queue_create_info.pQueuePriorities = queue_priorities;

                queue_create_infos[num_unique_queues] = queue_create_info;
                ++num_unique_queues;
            }
        }
        
        // ::::::::::::::::: ENABLE THE PHYSICAL DEVICE FEATURES WE ARE USING ::::::::::::::::: //
        // NOTE: Keep score_physical_device_and_check_required_features() up to date with this  //

        // The physical device features we are using get specified here
        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;
        device_features.geometryShader = VK_TRUE;

        VkPhysicalDeviceVulkan12Features vk12_features = {};
        vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12_features.bufferDeviceAddress = VK_TRUE;
        vk12_features.descriptorIndexing = VK_TRUE;
        vk12_features.descriptorBindingPartiallyBound = VK_TRUE;
        vk12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vk12_features.runtimeDescriptorArray = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vk13_features = {};
        vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13_features.dynamicRendering = VK_TRUE;
        vk13_features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceVulkan14Features vk14_features = {};
        vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vk14_features.maintenance5 = VK_TRUE;

        // // BufferDeviceAddress (buffer in shaders a la GPU pointers)
        // VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features = {};
        // buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        // buffer_device_address_features.bufferDeviceAddress = VK_TRUE;


        // The main device is created here, with the queues and features we just specified
        VkDeviceCreateInfo device_create_info = {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // pNext chain of features
        device_create_info.pNext = &vk14_features;
        vk14_features.pNext = &vk13_features;
        vk13_features.pNext = &vk12_features;
        vk12_features.pNext = NULL;
        // vk13_features.pNext = &buffer_device_address_features;
        // buffer_device_address_features.pNext = NULL;

        device_create_info.queueCreateInfoCount = num_unique_queues;
        device_create_info.pQueueCreateInfos = queue_create_infos;
        device_create_info.enabledExtensionCount = device_extensions_count;
        device_create_info.ppEnabledExtensionNames = device_extensions;
        device_create_info.pEnabledFeatures = &device_features;

        // Older Vulkan APIs had distinction between instance and device extensions.
        // Later versions don't need to specify extensions for the device 
        // but we do it anyway for compatability with older versions in case we ever downgrade for some platform.
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

        printf("Created Logcal Device.\n");

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
        printf("Vulkan Memory Allocator Initialised\n");

        SDL_assert(renderstate.vma_allocator != VK_NULL_HANDLE);
    }

    // Init FrameGraph subsystem
    FG_Init();

    // Create Swapchain, FrameGraph resources and pass descriptions
    renderstate.framegraph_rids.resources_created = 0;
    _Renderer_OnWindowResize();

    // Init per frame structures
    {
        VkFenceCreateInfo render_fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        VkCommandPoolCreateInfo gfx_cmdpool_create_info = {
            .sType             = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext             = NULL,
            .flags             = 0,
            .queueFamilyIndex  = renderstate.queue_family_indices.graphics_family
        };

        for (u32 i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateFence(renderstate.device, &render_fence_create_info, NULL, &renderstate.frames[i].rendering_complete_fence));
            VK_CHECK(vkCreateSemaphore(renderstate.device, &semaphore_create_info, NULL, &renderstate.frames[i].swapchain_image_acquired_semaphore));
            VK_CHECK(vkCreateCommandPool(renderstate.device, &gfx_cmdpool_create_info, NULL, &renderstate.frames[i].graphics_command_pool));
            VkCommandBufferAllocateInfo cmd_alloc_info = {
                .sType               = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext               = NULL,
                .commandPool         = renderstate.frames[i].graphics_command_pool,
                .level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount  = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(renderstate.device, &cmd_alloc_info, &renderstate.frames[i].graphics_command_buffer));
        }
    }

    return true;
}

void Renderer_Shutdown()
{
    printf("\n*********** Renderer_Shutdown() ***********\n");

    // Ensure device is not doing any work before destroying it's stuff.
    vkDeviceWaitIdle(renderstate.device);

    // Clean up per frame objects
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroyCommandPool(renderstate.device, renderstate.frames[i].graphics_command_pool, NULL);
        vkDestroySemaphore(renderstate.device, renderstate.frames[i].swapchain_image_acquired_semaphore, NULL);
        vkDestroyFence(renderstate.device, renderstate.frames[i].rendering_complete_fence, NULL);
    }

    // Shutdown FrameGraph subsystem
    FG_Shutdown();

    // Destroy resources for renderpasses
    DestroyResources();

    destroy_swapchain();

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
            printf(ANSI_CYAN "Vulkan cleanup was successful: No complaints from the enabled validation layers.\n" ANSI_RESET);
        }
        else
        {
            printf(ANSI_MAGENTA "Vulkan validation layer errors/warnings appeared :(\n" ANSI_RESET);
        }
    }

    // Report memory leaks of main tracker
    printf("\n***********  Memory Tracker Results ***********\n");
    check_tracker_for_memory_leaks(&renderstate.main.tt);
}

void Renderer_ListenToWindowEvent(SDL_Event event)
{
    switch (event.type)
    {
        case SDL_EVENT_WINDOW_RESIZED:
            _Renderer_OnWindowResize();   
            break;
        
        case SDL_EVENT_WINDOW_MINIMIZED:
            _Renderer_OnWindowMinimize();
            break;
    };
}

void _Renderer_OnWindowResize()
{
    vkDeviceWaitIdle(renderstate.device);
    create_or_recreate_swapchain();

    // Create pass resources and definitions
    if (renderstate.framegraph_rids.resources_created)
    {
        DestroyResources();
    }
    CreateResources();
}

void _Renderer_OnWindowMinimize()
{
    // TODO: Pause rendering on minimize
}

void Renderer_BeginFrame()
{
    /*  Build FrameGraph

        Queries game state to know which renderpasses to use.
        E.g. game.wearing_pyrovision_goggles would use swap the renderpass
        that renders flame particles as fire, to a renderpass that makes them bubbles
        or some shit.
    */

    // Empty pass descriptions
    renderstate.framegraph.pass_count = 0;
    memset(renderstate.framegraph.passes, 0, sizeof(renderstate.framegraph.passes));

    // TODO: Define basic pass for swapchain rendering

}

void Renderer_EndFrame()
{
    /*  Execute FrameGraph

        Gathers renderables from game state, based on flags, provides each
        pass their drawlists e.g. renderable with rig and unlit material would
        go to that specific pass.

        OR I can leave each execute callback to gather their relevant items themselves,
        which is probably easier
    
    */

    // Latency hiding:
    // Double/triple buffering command buffers allows us to start recording the next
    // frame's command buffer before the GPU has done with the current frame's one.
    u32 frame_in_flight = renderstate.frame_number % NUM_FRAMES_IN_FLIGHT;

    // Wait for rendering to be complete for this frame in flight.
    // NOTE: We don't reset the fence until after we know the swapchain does not need recreating.
    u64 sync_timeout_nanoseconds;
#ifdef NDEBUG
    sync_timeout_nanoseconds = UINT64_MAX;  // Wait forever in release mode
#else
    sync_timeout_nanoseconds = 1000000000UL;  // Only wait one second in debug mdoe to detect deadlocks/hangs
#endif
    VK_CHECK(vkWaitForFences(renderstate.device, 1, &renderstate.frames[frame_in_flight].rendering_complete_fence, VK_TRUE, sync_timeout_nanoseconds));

    // Get next swapchain image
    // NOTE: If another frame isn't finished with the swapchain image, the GPU must't execute commands on the next swapchain image.
    //       so it must have attained the image_aquired semaphore first.
    u32 swapchain_image_index;
    VkResult acquire_result = vkAcquireNextImageKHR(
        renderstate.device,
        renderstate.swapchain,
        sync_timeout_nanoseconds,
        renderstate.frames[frame_in_flight].swapchain_image_acquired_semaphore,
        VK_NULL_HANDLE,
        &swapchain_image_index
    );
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
    {
        // NOTE: This handles resizes explicitly for drivers that don't explicitly trigger
        // VK_ERROR_OUT_OF_DATE_KHR automatically on window resize (hence not triggering the SDL callback)
        // I don't get this issue on my laptop but it seems like it may be needed for some machines.

        // Return from draw function, marking the window to be resized and subtracting the framenumber
        // since we aren't drawing until the next main loop cycle.
        _Renderer_OnWindowResize();
        --renderstate.frame_number;
        return;
    }
    VK_CHECK(acquire_result);

    // Now we can safely reset the rendering complete fence.
    // NOTE: The fence is in place to make sure this frame in flight's graphics commands have finished executing on the GPU.
    VK_CHECK(vkResetFences(renderstate.device, 1, &renderstate.frames[frame_in_flight].rendering_complete_fence));
    
    // Reset command buffers by resetting the entire pool
    vkResetCommandPool(renderstate.device, renderstate.frames[frame_in_flight].graphics_command_pool, 0);



    // Setup graphics command buffer for one time submission
    VkCommandBufferBeginInfo graphics_cmd_begin_info = {
        .sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext             = NULL,
        .flags             = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo  = NULL
    };
    VkCommandBuffer gcmd = renderstate.frames[frame_in_flight].graphics_command_buffer;

    // Begin recording graphics commands
    //

    VK_CHECK(vkBeginCommandBuffer(gcmd, &graphics_cmd_begin_info));
    {
        // Swapchain as output color attachment
        {
            VkImageMemoryBarrier2 barrier = {
                .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext                = NULL,
                .srcStageMask         = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask        = VK_ACCESS_2_NONE,
                .dstStageMask         = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask        = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout            = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex  = renderstate.queue_family_indices.present_family,
                .dstQueueFamilyIndex  = renderstate.queue_family_indices.graphics_family,
                .image                = renderstate.swapchain_images[swapchain_image_index],
                .subresourceRange     = {
                    .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel    = 0,
                    .levelCount      = 1,
                    .baseArrayLayer  = 0,
                    .layerCount      = 1
                }
            };
            VkDependencyInfo dependency = {
                .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext                     = NULL,
                .dependencyFlags           = VK_DEPENDENCY_BY_REGION_BIT,
                // .memoryBarrierCount        =
                // .pMemoryBarriers           =
                // .bufferMemoryBarrierCount  =
                // .pBufferMemoryBarriers     =
                .imageMemoryBarrierCount   = 1,
                .pImageMemoryBarriers      = &barrier
            };
            vkCmdPipelineBarrier2(gcmd, &dependency);
        }

        // Rendering to swapchain for now


        // Swapchain to presentable format
        {
            VkImageMemoryBarrier2 barrier = {
                .sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext                = NULL,
                .srcStageMask         = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask        = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask         = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask        = VK_ACCESS_2_NONE,
                .oldLayout            = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout            = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex  = renderstate.queue_family_indices.present_family,
                .dstQueueFamilyIndex  = renderstate.queue_family_indices.graphics_family,
                .image                = renderstate.swapchain_images[swapchain_image_index],
                .subresourceRange     = {
                    .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel    = 0,
                    .levelCount      = 1,
                    .baseArrayLayer  = 0,
                    .layerCount      = 1
                }
            };
            VkDependencyInfo dependency = {
                .sType                     = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext                     = NULL,
                .dependencyFlags           = VK_DEPENDENCY_BY_REGION_BIT,
                // .memoryBarrierCount        =
                // .pMemoryBarriers           =
                // .bufferMemoryBarrierCount  =
                // .pBufferMemoryBarriers     =
                .imageMemoryBarrierCount   = 1,
                .pImageMemoryBarriers      = &barrier
            };
            vkCmdPipelineBarrier2(gcmd, &dependency);
        }
    }
    VK_CHECK(vkEndCommandBuffer(gcmd));

    //
    // End of graphics commands recording

    

    // Prepare submission of the command buffer to the queue.
    // - Requires waiting on the present semaphore (signalled when the swapchain is ready)
    // - We will signal the render semaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo gcmd_submit_info = {
        .sType          = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext          = NULL,
        .commandBuffer  = gcmd,
        .deviceMask     = 0
    };
    VkSemaphoreSubmitInfo gcmd_wait_info = {
        .sType        = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext        = NULL,
        .semaphore    = renderstate.frames[frame_in_flight].swapchain_image_acquired_semaphore,
        .value        = 1,
        .stageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .deviceIndex  = 0
    };
    VkSemaphoreSubmitInfo gcmd_signal_info = gcmd_wait_info;
    gcmd_signal_info.semaphore = renderstate.swapchain_image_rendering_complete_semaphores[swapchain_image_index];

    // Submit command buffer to the queue to execute it
    VkSubmitInfo2 submit_info = {
        .sType                     = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, 
        .pNext                     = NULL,
        .flags                     = 0,
        .waitSemaphoreInfoCount    = 1,
        .pWaitSemaphoreInfos       = &gcmd_wait_info,
        .commandBufferInfoCount    = 1,
        .pCommandBufferInfos       = &gcmd_submit_info,
        .signalSemaphoreInfoCount  = 1,
        .pSignalSemaphoreInfos     = &gcmd_signal_info
    };
    VK_CHECK(vkQueueSubmit2(renderstate.graphics_queue, 1, &submit_info, renderstate.frames[frame_in_flight].rendering_complete_fence));

    
    // Present to screen
    // (after waiting on the rendering complete semaphore so that all drawing is completed first)
    VkPresentInfoKHR present_info = {
        .sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext               = NULL,
        .waitSemaphoreCount  = 1,
        .pWaitSemaphores     = &renderstate.swapchain_image_rendering_complete_semaphores[swapchain_image_index],
        .swapchainCount      = 1,
        .pSwapchains         = &renderstate.swapchain,
        .pImageIndices       = &swapchain_image_index,
        .pResults            = NULL  // <- Only relevant when swapchainCount > 1
    };
    VkResult present_result = vkQueuePresentKHR(renderstate.presentation_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
    {
        _Renderer_OnWindowResize();
    }
    else
    {
        VK_CHECK(present_result);
    }

    // This happens at the very very end:
    ++renderstate.frame_number;
}

/////////////////

QueueFamilyIndices get_physical_device_queue_family_indices(VkPhysicalDevice physical_device)
{
    QueueFamilyIndices queue_family_indices = {};

    // Each queue family index initialized to UINT32_MAX so we can test for failure
    memset(&queue_family_indices, UINT32_MAX, sizeof(QueueFamilyIndices));

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
        printf("--- Queue family %d's bits: ", i);
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
        if (queue_families_support_for_graphics[i] && queue_family_indices.graphics_family == UINT32_MAX)
        {
            queue_family_indices.graphics_family = i;
        }
        if (queue_families_support_for_presentation[i] && queue_family_indices.present_family == UINT32_MAX)
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

    printf("--- name: %s\n", device_properties.deviceName);
    printf("--- supports up to Vulkan %d.%d.%d\n",
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
                printf("--- is dGPU... lovely :)\n");
                suitability_score += 1000;    
                break;

            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                printf("--- is iGPU... decent :^)\n");
                suitability_score += 500;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                // TODO: vGPU could be better or worse than the iGPU on a machine, need to check more features in the future (good enough for now)
                printf("--- is vGPU (virtual GPU)... who knows? TODO: Further checks for if it's a good vGPU e.g. cloud dGPU\n");
                suitability_score += 200;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                printf("--- is CPU... meh :(\n");
                suitability_score += 0;
                break;
            
            default:
                printf("--- dunno what type of device this is :(\n");
                suitability_score += 0;
        }
        
        // Check memory properties of this physical device
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        // Display memory heaps and types
        printf("--- Heaps: has %d memory heap.\n", memory_properties.memoryHeapCount);
        for (u32 i = 0; i < memory_properties.memoryHeapCount; ++i)
        {
            printf("--- - heap %i: %lu MiB, flags: ", i, memory_properties.memoryHeaps[i].size / (1024*1024));
            vklayer_print_memoryheapflagbits(memory_properties.memoryHeaps[i].flags);
        }

        printf("--- Memory types: %d types\n", memory_properties.memoryTypeCount);
        for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
        {
            printf("--- - type %d: from heap %d, flags: ", i, memory_properties.memoryTypes[i].heapIndex);
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
        // NOTE: Only double check vk13 and vk14 features, no need for earlier ones
        b32 all_required_features_supported = 0;
        if (supported_version_major > 1 || (supported_version_major == 1 && supported_version_minor >= 1))
        {
            VkPhysicalDeviceVulkan13Features vk13_features = {};
            vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

            VkPhysicalDeviceVulkan14Features vk14_features = {};
            vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

            VkPhysicalDeviceFeatures2 device_features = {};
            device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

            // pNext chain of features
            device_features.pNext = &vk14_features;
            vk14_features.pNext = &vk13_features;
            vk13_features.pNext = NULL;

            vkGetPhysicalDeviceFeatures2(physical_device, &device_features);

            all_required_features_supported = 1;
            printf("--- Push constant size limit: %d\n", device_properties.limits.maxPushConstantsSize);
            all_required_features_supported = all_required_features_supported && (device_properties.limits.maxPushConstantsSize >= 128);

            printf("--- Anisotropic filtering: %d\n", device_features.features.samplerAnisotropy);
            all_required_features_supported = all_required_features_supported  && device_features.features.samplerAnisotropy;
            if (device_features.features.samplerAnisotropy)
            {
                printf("--- - Max anisotropy: %f\n", device_properties.limits.maxSamplerAnisotropy);
            }
            
            printf("--- Synchronization 2: %d\n", vk13_features.synchronization2);
            all_required_features_supported = all_required_features_supported  && vk13_features.synchronization2;
            printf("--- Dynamic Rendering: %d\n", vk13_features.dynamicRendering);
            all_required_features_supported = all_required_features_supported  && vk13_features.dynamicRendering;
            
            // Maintenance5 means we don't need VkShaderModule.
            printf("--- Maintenance 5: %d\n", vk14_features.maintenance5);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance5;
            printf("--- Maintenance 6: %d\n", vk14_features.maintenance6);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance6;
        }

        // Requires the necessary queue families
        QueueFamilyIndices indices = get_physical_device_queue_family_indices(physical_device);
        b32 all_required_queues_supported = 
                indices.graphics_family != UINT32_MAX &&
                indices.present_family != UINT32_MAX;

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
            SwapchainSupportDetails details = get_and_alloc_swap_chain_support_details(physical_device);
            is_swapchain_adequate = details.format_count > 0 && details.present_mode_count > 0;
            free_swap_chain_support_details(details);
        }

        printf("--- Checking against requirements: VersionUpToDate:%d, Features:%d, Queues:%d, Extensions:%d, SwapChain:%d\n",
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

// NOTE: Need to free SwapchainSupportDetails.formats and .present_modes after use
SwapchainSupportDetails get_and_alloc_swap_chain_support_details(VkPhysicalDevice physical_device)
{
    SwapchainSupportDetails details = {};

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

void free_swap_chain_support_details(SwapchainSupportDetails details)
{
    if (details.formats)       L_free(details.formats, &renderstate.main.tt);
    if (details.present_modes) L_free(details.present_modes, &renderstate.main.tt);
}

void create_or_recreate_swapchain()
{
    // NOTE: If a swapchain already exists, Vulkan wants the handle to the old swapchain
    // passed to the swapchain create info of the new one.
    // Hence create_or_recreate instead of just destroy() then create() when one already exists.

    VkSurfaceFormatKHR chosen_format;
    VkPresentModeKHR chosen_present_mode;
    VkExtent2D chosen_swap_extent;

    // Get support details for swap chain
    SwapchainSupportDetails details = get_and_alloc_swap_chain_support_details(renderstate.physical_device);

    // Choose format
    SDL_assert(details.format_count > 0);
    int chosen_format_index = 0;
    for (u32 i = 0; i < details.format_count; ++i)
    {
        if (details.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            details.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen_format_index = i;
        }
    }
    chosen_format = details.formats[chosen_format_index];

    // Choose present mode
    chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Only FIFO is guarunteed to be available
    for (u32 i = 0; i < details.present_mode_count; ++i)
    {
        if (renderstate.uncapped_fps)
        {
            // Mailbox (aka "triplebuffering") means minimal latency without screen-tearing
            // So we'll use this if it's available
            if (details.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                chosen_present_mode = details.present_modes[i];
            }
        }
        else
        {
            // Relaxed means if we miss the vsync slightly, we still submit
            // in that case the screen will have tearing, but it means the fps
            // doesn't get halved by needing to wait for the next "vblank".
            if (details.present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            {
                chosen_present_mode = details.present_modes[i];
            }
        }
    }

    // Choose swapchain image extents
    if (details.capabilities.currentExtent.width != UINT32_MAX)
    {
        chosen_swap_extent = details.capabilities.currentExtent;
    }
    else
    {
        // Requesting in pixel coordinates not screen coordinates because some HiDPI displays make a distinction there
        // and we want to actually render to each and every pixel available on the monitor
        int width, height;
        SDL_assert(
            SDL_GetWindowSizeInPixels(renderstate.window, &width, &height)
        );

        VkExtent2D actual_extent = { (u32)width, (u32)height };

        // Must be clamped between the min and max extents allowed by the implementation
        actual_extent.width = SDL_clamp(actual_extent.width,
            details.capabilities.minImageExtent.width,
            details.capabilities.maxImageExtent.width
        );
        actual_extent.height = SDL_clamp(actual_extent.height,
            details.capabilities.minImageExtent.height,
            details.capabilities.maxImageExtent.height
        );

        chosen_swap_extent = actual_extent;
    }

    // Request (minImageCount + 1) images so that there is always an image we can immediately aquire to render to.
    // NOTE: minImageCount is the minimum amount for the swapchain implementation to function.
    u32 min_image_count = details.capabilities.minImageCount + 1;

    // Then make sure this doesn't push us over the maxImageCount
    // NOTE: maxImageCount of 0 is a special value for no maximum.
    if (details.capabilities.maxImageCount > 0 &&
        min_image_count > details.capabilities.maxImageCount)
    {
        min_image_count = details.capabilities.maxImageCount;
    }


    // Swapchain Create Info
    VkSwapchainKHR old_swapchain = renderstate.swapchain;
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext                  = NULL,
        .flags                  = 0,
        .surface                = renderstate.surface,
        .minImageCount          = min_image_count,
        .imageFormat            = chosen_format.format,
        .imageColorSpace        = chosen_format.colorSpace,
        .imageExtent            = chosen_swap_extent,
        .imageArrayLayers       = 1,  // One layer since we aren't making a stereoscopic 3D application
        .imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,  // Transfer for blit/copy operations

        // NOTE: With explicit queue ownership transfers of the swapchain images via pipeline barriers between
        // graphics queue and present queue commands involving the swapchain,
        // it is then safe to use VK_SHARING_MODE_EXCLUSIVE even if present queue != graphics queue.
        .imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount  = 0,
        .pQueueFamilyIndices    = NULL,

        .preTransform           = details.capabilities.currentTransform,
        .compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,  // Alpha is opaque, i.e. no blending with other windows
        .presentMode            = chosen_present_mode,

        // When another window is partially in the way, we don't care what the covered pixels colours are
        // (unless you are reading from them later for some special reason, I'm not)
        .clipped                = VK_TRUE,


        // Passing the old swapchain can allow the driver to reuse some resources
        // old_swapchain will be VK_NULL_HANDLE on first creation since an old swapchain does not exist.
        .oldSwapchain           = old_swapchain
    };

    // Create the (new) swapchain
    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(renderstate.device, &swapchain_create_info, NULL, &new_swapchain));
    free_swap_chain_support_details(details);

    // Save the old format to check whether the new swapchain's format is the same or not.
    // Because we only need to recreate graphics pipelines if the format changes.
    // Window resizes don't require recreating piplines, since we use dynamic pipeline state for that.
    VkFormat old_format = renderstate.swapchain_image_format;

    // Save the chosen format and extent so we can copy/transfer correctly to it later
    renderstate.swapchain_image_format = chosen_format.format;
    renderstate.swapchain_extent = chosen_swap_extent;

    // Destroy old swapchain if it exists
    if (old_swapchain != VK_NULL_HANDLE)
    {
        // Vulkan never implicitly destroys the old swapchain we passed to the new create info, we still must destroy it.
        destroy_swapchain();
    }
    renderstate.swapchain = new_swapchain;


    // Get number of swapchain images
    vkGetSwapchainImagesKHR(renderstate.device, renderstate.swapchain, &renderstate.swapchain_image_count, NULL);
    SDL_assert(renderstate.swapchain_image_count <= MAX_SWAPCHAIN_IMAGE_COUNT);

    // Retrieve the handles of the images created by the swapchaip
    memset(renderstate.swapchain_images, 0, sizeof(renderstate.swapchain_images));
    vkGetSwapchainImagesKHR(renderstate.device, renderstate.swapchain, &renderstate.swapchain_image_count, renderstate.swapchain_images);

    printf("Swapchain created.\n");
    printf("- pixel resolution(%d, %d)\n", renderstate.swapchain_extent.width, renderstate.swapchain_extent.height);
    
    
    // Create Image Views for SwapChain
    memset(renderstate.swapchain_image_views, 0, sizeof(renderstate.swapchain_image_views));
    for (u32 i = 0; i < renderstate.swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo image_view_create_info = {
            .sType             = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext             = NULL,
            .flags             = 0,
            .image             = renderstate.swapchain_images[i],
            .viewType          = VK_IMAGE_VIEW_TYPE_2D,
            .format            = renderstate.swapchain_image_format,

            // Use no swizzle for the color components
            .components        = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },

            // Specify as color target with no mipmapping
            .subresourceRange  = {
                .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel    = 0,
                .levelCount      = 1,
                .baseArrayLayer  = 0,
                .layerCount      = 1
            }
        };
        VK_CHECK(vkCreateImageView(renderstate.device, &image_view_create_info, NULL, &renderstate.swapchain_image_views[i]));
    }


    // Create Semaphores for SwapChain
    memset(renderstate.swapchain_image_rendering_complete_semaphores, 0, sizeof(renderstate.swapchain_image_rendering_complete_semaphores));

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0
    };
    for (u32 i = 0; i < renderstate.swapchain_image_count; ++i)
    {
        VK_CHECK(vkCreateSemaphore(renderstate.device, &semaphore_create_info, NULL, &renderstate.swapchain_image_rendering_complete_semaphores[i]));
    }
    

    // Swapchain creation is completed
    printf("- created %d swapchain image views.\n", renderstate.swapchain_image_count);
}

void destroy_swapchain()
{
    // Destroy Image Views and Semaphores
    for (u32 i = 0; i < renderstate.swapchain_image_count; ++i)
    {
        vkDestroyImageView(renderstate.device, renderstate.swapchain_image_views[i], NULL);
        vkDestroySemaphore(renderstate.device, renderstate.swapchain_image_rendering_complete_semaphores[i], NULL);
    }

    // Swapchain images are automatically destroyed when swapchain is destroyed due to ownership
    vkDestroySwapchainKHR(renderstate.device, renderstate.swapchain, NULL);
}
