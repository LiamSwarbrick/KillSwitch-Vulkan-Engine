#include "renderer/renderer.h"
#include "internal_state.h"
#include "renderpasses/metadata.h"

#include "SDL3/SDL_vulkan.h"

RenderState renderstate;

// STB DS for hash maps (pipeilne hashing), with the main thread alloc tracker.
void* external_realloc(void* ptr, size_t size) { return L_realloc(ptr, size, &renderstate.main.tt); }
void external_free(void* ptr) { return L_free(ptr, &renderstate.main.tt); }
#define STBDS_REALLOC(context,ptr,size) external_realloc(ptr, size)
#define STBDS_FREE(context,ptr)         external_free(ptr)
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"  // Pipeline keying using this, I'm the STB_DS_IMPLEMENTATION here


// NOTE(Liam): The only mutable internal state for renderer is this renderstate.
// All other global state here should be const
// Except Physical Device Features (because of setting the pNext chain)

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


// Vulkan Physical Device Features
// FUTURE: In future, we could dynamically change our featureset based on the user's hardware
VkPhysicalDeviceFeatures physical_device_features = {
    // .geometryShader    = VK_TRUE,
    .samplerAnisotropy = VK_TRUE,
    .shaderInt64       = VK_TRUE
};
VkPhysicalDeviceVulkan12Features vk12_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = NULL,
    .descriptorIndexing                           = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingPartiallyBound              = VK_TRUE,
    .runtimeDescriptorArray                       = VK_TRUE,
    .scalarBlockLayout                            = VK_TRUE,
    .bufferDeviceAddress                          = VK_TRUE
};
VkPhysicalDeviceVulkan13Features vk13_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &vk12_features,
    .synchronization2 = VK_TRUE,
    .dynamicRendering = VK_TRUE
};
VkPhysicalDeviceVulkan14Features vk14_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
    .pNext = &vk13_features,
    .maintenance5 = VK_TRUE
};
void* physical_device_features_pNext_chain = &vk14_features;  // Set pNext chain for VkDeviceCreateInfo


static VKAPI_ATTR VkBool32 VKAPI_CALL InternalVulkanDebugCallback(
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
        *(volatile int*)0 = 0;  // Crash the program so I can backtrace
        renderstate.program_caused_vulkan_validation_layer_errors = 1;
        break;
        default: break;
    }

    return VK_FALSE;
}

void Renderer_Init(const Renderer_InitInfo* info)
{
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
        exit(1);
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
            exit(1);
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

        // TODO: If using optional functionality, query available instance extensions first with vkEnumerateInstanceExtensionProperties()
        // {
        //     u32 extension_count = 0;
        //     vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
        //     etc...
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
            exit(1);
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
            exit(1);
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
            exit(1);
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
        printf("Transfer queue family at index %d\n", renderstate.queue_family_indices.transfer_family);

        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            SDL_assert(renderstate.queue_family_indices.array[i] != UINT32_MAX);
        }
    }

    // Create Logical Device
    {
        renderstate.device = VK_NULL_HANDLE;

        // Create the queues we will submit commands to
        VkDeviceQueueCreateInfo queue_create_infos[NUM_QUEUE_FAMILY_INDICES] = {};
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
        
        // ENABLE THE PHYSICAL DEVICE FEATURES WE ARE USING
        //

        // The main device is created here, with the queues and features we just specified
        VkDeviceCreateInfo device_create_info = {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext = physical_device_features_pNext_chain;  // pNext chain of features
        device_create_info.queueCreateInfoCount     = num_unique_queues;
        device_create_info.pQueueCreateInfos        = queue_create_infos;
        device_create_info.enabledExtensionCount    = device_extensions_count;
        device_create_info.ppEnabledExtensionNames  = device_extensions;
        device_create_info.pEnabledFeatures         = &physical_device_features;

        // Older Vulkan APIs had distinction between instance and device extensions.
        // Later versions don't need to specify extensions for the device 
        // but we do it anyway for compatability with older versions in case we ever downgrade for some platform.
        device_create_info.enabledLayerCount   = validation_layers_count;
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
        vkGetDeviceQueue(renderstate.device, renderstate.queue_family_indices.transfer_family, 0, &renderstate.transfer_queue);

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
        allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  
        VmaVulkanFunctions vulkan_functions;
        VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocator_create_info, &vulkan_functions));
        allocator_create_info.pVulkanFunctions = &vulkan_functions;

        VK_CHECK(vmaCreateAllocator(&allocator_create_info, &renderstate.vma_allocator));
        printf("Vulkan Memory Allocator Initialised\n");

        SDL_assert(renderstate.vma_allocator != VK_NULL_HANDLE);
    }

    // Transfer command buffers per thread
    {
        // Create mutex for syncing multithreaded submits to the transfer queue
        renderstate.transfer_queue_mutex = SDL_CreateMutex();
        SDL_assert(renderstate.transfer_queue_mutex);

        // Command pool/buffer for this worker thread (Main thread only at the moment)
        create_thread_staging_objects(&renderstate.main.staging_objects);
        // FUTURE: Currently just the main thread will do all the transfering
        // But this should just work with multithreading when using thread_safe_submit_cmd()
    }

    // Init Subsystems
    //
    //   FrameGraph:
    //   - Orders and executes renderpasses with automatic resource synchronisation (pipeline barriers).
    //   - Contains ResourceRegistry for all textures and buffers (the GPU resources)
    //
    //   Pipeline Keying:
    //   - During rendering, if a renderable requires a new pipeline, it lazily creates it.
    //   - Also responsible for shaders.
    //
    FG_Init();
    PK_Init(&renderstate.pipeline_map);

    renderstate.rids.window_resources_created = 0;
    renderstate.rids.startup_resources_created = 0;
    _Renderer_OnWindowResize();  // <-- Creates swapchain and window dependent resources
    CreateOrRecreateResources(FG_RESOURCE_FLAGS_ON_STARTUP);

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

    // Shutdown Pipeline Keying and Frame Graph subsystems
    PK_Shutdown(&renderstate.pipeline_map, renderstate.device);
    FG_Shutdown();

    // Destroy GPU resources
    DestroyResources();

    destroy_swapchain();

    // Thread staging objects (only one thread for now)
    destroy_thread_staging_objects(&renderstate.main.staging_objects);
    SDL_DestroyMutex(renderstate.transfer_queue_mutex);

    // Destroy fundamental Vulkan objects
    vmaDestroyAllocator(renderstate.vma_allocator);
    vkDestroyDevice(renderstate.device, NULL);
    vkDestroySurfaceKHR(renderstate.instance, renderstate.surface, NULL);
    if (renderstate.using_validation_layers)
    {
        vkDestroyDebugUtilsMessengerEXT(renderstate.instance, renderstate.debug_messenger, NULL);
    }
    vkDestroyInstance(renderstate.instance, NULL);

    // Destroy Window Surface (core destroys the actual SDL window)
    SDL_DestroyWindowSurface(renderstate.window);

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
    // TODO: Change this so that it pauses rendering until stopped resizing
    // This way it'll performs one single resize at the end instead of constantly while resizing
    // Cuz right now it's super slow since resizing it reallocating huge render targets over and over...

    vkDeviceWaitIdle(renderstate.device);
    create_or_recreate_swapchain();

    // Create or recreate window size dependent resources
    CreateOrRecreateResources(FG_RESOURCE_FLAGS_WINDOW_DEPENDENT);
}

void _Renderer_OnWindowMinimize()
{
    // TODO: Pause rendering on minimize
}


void Renderer_DrawFrame()
{
    /*  Get current swapchain image, and wait on sync structures

        This happens before building the frame graph, since we need to know
        what swapchain image index this frame is using.
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
    retry_with_resized_window:
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
        // Haven't personally encountered this issue on my laptop but it seems like it may be needed for some machines.

        // Window to be resized.
        _Renderer_OnWindowResize();
        
        // Swapchain was out of date, so try again.
        goto retry_with_resized_window;
    }
    VK_CHECK(acquire_result);

    // Now we can safely reset the rendering complete fence.
    // NOTE: The fence is in place to make sure this frame in flight's graphics commands have finished executing on the GPU.
    VK_CHECK(vkResetFences(renderstate.device, 1, &renderstate.frames[frame_in_flight].rendering_complete_fence));
    
    // Reset command buffers by resetting the entire pool
    vkResetCommandPool(renderstate.device, renderstate.frames[frame_in_flight].graphics_command_pool, 0);


    
    /*  Build FrameGraph

        Queries game state to know which renderpasses to use.
        E.g. game.wearing_pyrovision_goggles would use swap the renderpass
        that renders flame particles as fire, to a renderpass that makes them bubbles
        or some shit.
    */

    FG_Empty();

    // Swapchain pass
    uint32_t swapchain_image_resource_id = renderstate.rids.swapchain_image_rids[swapchain_image_index];
    uint32_t swapchain_pass;
    {
        // Temporary basic pass for swapchain rendering
        RenderPassDesc swapchain_pass_desc = (RenderPassDesc){
            .debug_name = "Swapchain Pass",
            .input_count = 0,
            .inputs = {},
            .output_count = 1,
            .outputs = {
                {
                    .rid = swapchain_image_resource_id,
                    .usage_flags = FG_USAGE_COLOR,
                    .sampler_type = FG_SAMPLER_NOT_SAMPLABLE,

                    .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .stage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                    .clear_value = { .color = { .float32 = { 0.392f, 0.584f, 0.929f, 0.0f } } }
                }
            },

            .is_compute = 0,
            .render_area = { .offset = { 0, 0 }, .extent = renderstate.swapchain_extent },
            
            .execute_callback = SwapchainPass_Execute
        };
        swapchain_pass = FG_AddPass(swapchain_pass_desc, PASS_TYPE_SWAPCHAIN_PASS);
    }



    /*  Execute FrameGraph

        Gathers renderables from game state, based on flags, provides each
        pass their drawlists e.g. renderable with rig and unlit material would
        go to that specific pass.

        OR I can leave each execute callback to gather their relevant items themselves,
        which is probably easier
    
    */

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
        UpdateGlobalSceneData();
        FG_CmdRenderFrame(gcmd);
        FG_CmdTransitionSwapchainForPresentation(gcmd, swapchain_image_resource_id);
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
    for (u32 i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
    {
        queue_family_indices.array[i] = UINT32_MAX;
    }

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    const uint32_t max_queue_families = 32;  // I want to abuse the stack with fixed arrays to keep malloc calls to a minimum. Doing this across the codebase would add up over time.
    SDL_assert(queue_family_count < max_queue_families);

    VkQueueFamilyProperties queue_families[max_queue_families] = {};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    // Check for support of each required queue for each queue family
    b32 queue_families_support_for_graphics[max_queue_families]     = {};
    b32 queue_families_support_for_presentation[max_queue_families] = {};
    b32 queue_families_support_for_transfer[max_queue_families]     = {};
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

        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            queue_families_support_for_transfer[i] = 1;
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
        if (queue_families_support_for_transfer[i] && queue_family_indices.transfer_family == UINT32_MAX)
        {
            queue_family_indices.transfer_family = i;
        }
        // etc.
    }

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
            // Query available features for this physical device
            VkPhysicalDeviceVulkan12Features available_vk12_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = NULL
            };
            VkPhysicalDeviceVulkan13Features available_vk13_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .pNext = &available_vk12_features,
            };
            VkPhysicalDeviceVulkan14Features available_vk14_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
                .pNext = &available_vk13_features
            };
            VkPhysicalDeviceFeatures2 available_device_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &available_vk14_features
            };
            vkGetPhysicalDeviceFeatures2(physical_device, &available_device_features);

            // Check required features are available
            //
            // NOTE: Used multicursor to manually copying all features from vulkan_core.h instead of relying on looping over a struct via pointer arithmetic
            //       because pointer arithmetic assumes tight struct packing rules, so this would be unreliable for structs with the structure type and pNext at the start.
            //
            // BY THE WAY: NOT A YANDEREDEV MOMENT '(ᗒᗣᗕ)՞
            b32 any_feature_missing = 0;
            {
                // VkPhysicalDeviceFeatures
                any_feature_missing = any_feature_missing
                || (physical_device_features.robustBufferAccess && !available_device_features.features.robustBufferAccess)
                || (physical_device_features.fullDrawIndexUint32 && !available_device_features.features.fullDrawIndexUint32)
                || (physical_device_features.imageCubeArray && !available_device_features.features.imageCubeArray)
                || (physical_device_features.independentBlend && !available_device_features.features.independentBlend)
                || (physical_device_features.geometryShader && !available_device_features.features.geometryShader)
                || (physical_device_features.tessellationShader && !available_device_features.features.tessellationShader)
                || (physical_device_features.sampleRateShading && !available_device_features.features.sampleRateShading)
                || (physical_device_features.dualSrcBlend && !available_device_features.features.dualSrcBlend)
                || (physical_device_features.logicOp && !available_device_features.features.logicOp)
                || (physical_device_features.multiDrawIndirect && !available_device_features.features.multiDrawIndirect)
                || (physical_device_features.drawIndirectFirstInstance && !available_device_features.features.drawIndirectFirstInstance)
                || (physical_device_features.depthClamp && !available_device_features.features.depthClamp)
                || (physical_device_features.depthBiasClamp && !available_device_features.features.depthBiasClamp)
                || (physical_device_features.fillModeNonSolid && !available_device_features.features.fillModeNonSolid)
                || (physical_device_features.depthBounds && !available_device_features.features.depthBounds)
                || (physical_device_features.wideLines && !available_device_features.features.wideLines)
                || (physical_device_features.largePoints && !available_device_features.features.largePoints)
                || (physical_device_features.alphaToOne && !available_device_features.features.alphaToOne)
                || (physical_device_features.multiViewport && !available_device_features.features.multiViewport)
                || (physical_device_features.samplerAnisotropy && !available_device_features.features.samplerAnisotropy)
                || (physical_device_features.textureCompressionETC2 && !available_device_features.features.textureCompressionETC2)
                || (physical_device_features.textureCompressionASTC_LDR && !available_device_features.features.textureCompressionASTC_LDR)
                || (physical_device_features.textureCompressionBC && !available_device_features.features.textureCompressionBC)
                || (physical_device_features.occlusionQueryPrecise && !available_device_features.features.occlusionQueryPrecise)
                || (physical_device_features.pipelineStatisticsQuery && !available_device_features.features.pipelineStatisticsQuery)
                || (physical_device_features.vertexPipelineStoresAndAtomics && !available_device_features.features.vertexPipelineStoresAndAtomics)
                || (physical_device_features.fragmentStoresAndAtomics && !available_device_features.features.fragmentStoresAndAtomics)
                || (physical_device_features.shaderTessellationAndGeometryPointSize && !available_device_features.features.shaderTessellationAndGeometryPointSize)
                || (physical_device_features.shaderImageGatherExtended && !available_device_features.features.shaderImageGatherExtended)
                || (physical_device_features.shaderStorageImageExtendedFormats && !available_device_features.features.shaderStorageImageExtendedFormats)
                || (physical_device_features.shaderStorageImageMultisample && !available_device_features.features.shaderStorageImageMultisample)
                || (physical_device_features.shaderStorageImageReadWithoutFormat && !available_device_features.features.shaderStorageImageReadWithoutFormat)
                || (physical_device_features.shaderStorageImageWriteWithoutFormat && !available_device_features.features.shaderStorageImageWriteWithoutFormat)
                || (physical_device_features.shaderUniformBufferArrayDynamicIndexing && !available_device_features.features.shaderUniformBufferArrayDynamicIndexing)
                || (physical_device_features.shaderSampledImageArrayDynamicIndexing && !available_device_features.features.shaderSampledImageArrayDynamicIndexing)
                || (physical_device_features.shaderStorageBufferArrayDynamicIndexing && !available_device_features.features.shaderStorageBufferArrayDynamicIndexing)
                || (physical_device_features.shaderStorageImageArrayDynamicIndexing && !available_device_features.features.shaderStorageImageArrayDynamicIndexing)
                || (physical_device_features.shaderClipDistance && !available_device_features.features.shaderClipDistance)
                || (physical_device_features.shaderCullDistance && !available_device_features.features.shaderCullDistance)
                || (physical_device_features.shaderFloat64 && !available_device_features.features.shaderFloat64)
                || (physical_device_features.shaderInt64 && !available_device_features.features.shaderInt64)
                || (physical_device_features.shaderInt16 && !available_device_features.features.shaderInt16)
                || (physical_device_features.shaderResourceResidency && !available_device_features.features.shaderResourceResidency)
                || (physical_device_features.shaderResourceMinLod && !available_device_features.features.shaderResourceMinLod)
                || (physical_device_features.sparseBinding && !available_device_features.features.sparseBinding)
                || (physical_device_features.sparseResidencyBuffer && !available_device_features.features.sparseResidencyBuffer)
                || (physical_device_features.sparseResidencyImage2D && !available_device_features.features.sparseResidencyImage2D)
                || (physical_device_features.sparseResidencyImage3D && !available_device_features.features.sparseResidencyImage3D)
                || (physical_device_features.sparseResidency2Samples && !available_device_features.features.sparseResidency2Samples)
                || (physical_device_features.sparseResidency4Samples && !available_device_features.features.sparseResidency4Samples)
                || (physical_device_features.sparseResidency8Samples && !available_device_features.features.sparseResidency8Samples)
                || (physical_device_features.sparseResidency16Samples && !available_device_features.features.sparseResidency16Samples)
                || (physical_device_features.sparseResidencyAliased && !available_device_features.features.sparseResidencyAliased)
                || (physical_device_features.variableMultisampleRate && !available_device_features.features.variableMultisampleRate)
                || (physical_device_features.inheritedQueries && !available_device_features.features.inheritedQueries);

                // VkPhysicalDeviceVulkan12Features
                any_feature_missing = any_feature_missing
                || (vk12_features.samplerMirrorClampToEdge && !available_vk12_features.samplerMirrorClampToEdge)
                || (vk12_features.drawIndirectCount && !available_vk12_features.drawIndirectCount)
                || (vk12_features.storageBuffer8BitAccess && !available_vk12_features.storageBuffer8BitAccess)
                || (vk12_features.uniformAndStorageBuffer8BitAccess && !available_vk12_features.uniformAndStorageBuffer8BitAccess)
                || (vk12_features.storagePushConstant8 && !available_vk12_features.storagePushConstant8)
                || (vk12_features.shaderBufferInt64Atomics && !available_vk12_features.shaderBufferInt64Atomics)
                || (vk12_features.shaderSharedInt64Atomics && !available_vk12_features.shaderSharedInt64Atomics)
                || (vk12_features.shaderFloat16 && !available_vk12_features.shaderFloat16)
                || (vk12_features.shaderInt8 && !available_vk12_features.shaderInt8)
                || (vk12_features.descriptorIndexing && !available_vk12_features.descriptorIndexing)
                || (vk12_features.shaderInputAttachmentArrayDynamicIndexing && !available_vk12_features.shaderInputAttachmentArrayDynamicIndexing)
                || (vk12_features.shaderUniformTexelBufferArrayDynamicIndexing && !available_vk12_features.shaderUniformTexelBufferArrayDynamicIndexing)
                || (vk12_features.shaderStorageTexelBufferArrayDynamicIndexing && !available_vk12_features.shaderStorageTexelBufferArrayDynamicIndexing)
                || (vk12_features.shaderUniformBufferArrayNonUniformIndexing && !available_vk12_features.shaderUniformBufferArrayNonUniformIndexing)
                || (vk12_features.shaderSampledImageArrayNonUniformIndexing && !available_vk12_features.shaderSampledImageArrayNonUniformIndexing)
                || (vk12_features.shaderStorageBufferArrayNonUniformIndexing && !available_vk12_features.shaderStorageBufferArrayNonUniformIndexing)
                || (vk12_features.shaderStorageImageArrayNonUniformIndexing && !available_vk12_features.shaderStorageImageArrayNonUniformIndexing)
                || (vk12_features.shaderInputAttachmentArrayNonUniformIndexing && !available_vk12_features.shaderInputAttachmentArrayNonUniformIndexing)
                || (vk12_features.shaderUniformTexelBufferArrayNonUniformIndexing && !available_vk12_features.shaderUniformTexelBufferArrayNonUniformIndexing)
                || (vk12_features.shaderStorageTexelBufferArrayNonUniformIndexing && !available_vk12_features.shaderStorageTexelBufferArrayNonUniformIndexing)
                || (vk12_features.descriptorBindingUniformBufferUpdateAfterBind && !available_vk12_features.descriptorBindingUniformBufferUpdateAfterBind)
                || (vk12_features.descriptorBindingSampledImageUpdateAfterBind && !available_vk12_features.descriptorBindingSampledImageUpdateAfterBind)
                || (vk12_features.descriptorBindingStorageImageUpdateAfterBind && !available_vk12_features.descriptorBindingStorageImageUpdateAfterBind)
                || (vk12_features.descriptorBindingStorageBufferUpdateAfterBind && !available_vk12_features.descriptorBindingStorageBufferUpdateAfterBind)
                || (vk12_features.descriptorBindingUniformTexelBufferUpdateAfterBind && !available_vk12_features.descriptorBindingUniformTexelBufferUpdateAfterBind)
                || (vk12_features.descriptorBindingStorageTexelBufferUpdateAfterBind && !available_vk12_features.descriptorBindingStorageTexelBufferUpdateAfterBind)
                || (vk12_features.descriptorBindingUpdateUnusedWhilePending && !available_vk12_features.descriptorBindingUpdateUnusedWhilePending)
                || (vk12_features.descriptorBindingPartiallyBound && !available_vk12_features.descriptorBindingPartiallyBound)
                || (vk12_features.descriptorBindingVariableDescriptorCount && !available_vk12_features.descriptorBindingVariableDescriptorCount)
                || (vk12_features.runtimeDescriptorArray && !available_vk12_features.runtimeDescriptorArray)
                || (vk12_features.samplerFilterMinmax && !available_vk12_features.samplerFilterMinmax)
                || (vk12_features.scalarBlockLayout && !available_vk12_features.scalarBlockLayout)
                || (vk12_features.imagelessFramebuffer && !available_vk12_features.imagelessFramebuffer)
                || (vk12_features.uniformBufferStandardLayout && !available_vk12_features.uniformBufferStandardLayout)
                || (vk12_features.shaderSubgroupExtendedTypes && !available_vk12_features.shaderSubgroupExtendedTypes)
                || (vk12_features.separateDepthStencilLayouts && !available_vk12_features.separateDepthStencilLayouts)
                || (vk12_features.hostQueryReset && !available_vk12_features.hostQueryReset)
                || (vk12_features.timelineSemaphore && !available_vk12_features.timelineSemaphore)
                || (vk12_features.bufferDeviceAddress && !available_vk12_features.bufferDeviceAddress)
                || (vk12_features.bufferDeviceAddressCaptureReplay && !available_vk12_features.bufferDeviceAddressCaptureReplay)
                || (vk12_features.bufferDeviceAddressMultiDevice && !available_vk12_features.bufferDeviceAddressMultiDevice)
                || (vk12_features.vulkanMemoryModel && !available_vk12_features.vulkanMemoryModel)
                || (vk12_features.vulkanMemoryModelDeviceScope && !available_vk12_features.vulkanMemoryModelDeviceScope)
                || (vk12_features.vulkanMemoryModelAvailabilityVisibilityChains && !available_vk12_features.vulkanMemoryModelAvailabilityVisibilityChains)
                || (vk12_features.shaderOutputViewportIndex && !available_vk12_features.shaderOutputViewportIndex)
                || (vk12_features.shaderOutputLayer && !available_vk12_features.shaderOutputLayer)
                || (vk12_features.subgroupBroadcastDynamicId && !available_vk12_features.subgroupBroadcastDynamicId);
                
                // VkPhysicalDeviceVulkan13Features
                any_feature_missing = any_feature_missing
                || (vk13_features.robustImageAccess && !available_vk13_features.robustImageAccess)
                || (vk13_features.inlineUniformBlock && !available_vk13_features.inlineUniformBlock)
                || (vk13_features.descriptorBindingInlineUniformBlockUpdateAfterBind && !available_vk13_features.descriptorBindingInlineUniformBlockUpdateAfterBind)
                || (vk13_features.pipelineCreationCacheControl && !available_vk13_features.pipelineCreationCacheControl)
                || (vk13_features.privateData && !available_vk13_features.privateData)
                || (vk13_features.shaderDemoteToHelperInvocation && !available_vk13_features.shaderDemoteToHelperInvocation)
                || (vk13_features.shaderTerminateInvocation && !available_vk13_features.shaderTerminateInvocation)
                || (vk13_features.subgroupSizeControl && !available_vk13_features.subgroupSizeControl)
                || (vk13_features.computeFullSubgroups && !available_vk13_features.computeFullSubgroups)
                || (vk13_features.synchronization2 && !available_vk13_features.synchronization2)
                || (vk13_features.textureCompressionASTC_HDR && !available_vk13_features.textureCompressionASTC_HDR)
                || (vk13_features.shaderZeroInitializeWorkgroupMemory && !available_vk13_features.shaderZeroInitializeWorkgroupMemory)
                || (vk13_features.dynamicRendering && !available_vk13_features.dynamicRendering)
                || (vk13_features.shaderIntegerDotProduct && !available_vk13_features.shaderIntegerDotProduct)
                || (vk13_features.maintenance4 && !available_vk13_features.maintenance4);

                // VkPhysicalDeviceVulkan14Features
                any_feature_missing = any_feature_missing
                || (vk14_features.globalPriorityQuery && !available_vk14_features.globalPriorityQuery)
                || (vk14_features.shaderSubgroupRotate && !available_vk14_features.shaderSubgroupRotate)
                || (vk14_features.shaderSubgroupRotateClustered && !available_vk14_features.shaderSubgroupRotateClustered)
                || (vk14_features.shaderFloatControls2 && !available_vk14_features.shaderFloatControls2)
                || (vk14_features.shaderExpectAssume && !available_vk14_features.shaderExpectAssume)
                || (vk14_features.rectangularLines && !available_vk14_features.rectangularLines)
                || (vk14_features.bresenhamLines && !available_vk14_features.bresenhamLines)
                || (vk14_features.smoothLines && !available_vk14_features.smoothLines)
                || (vk14_features.stippledRectangularLines && !available_vk14_features.stippledRectangularLines)
                || (vk14_features.stippledBresenhamLines && !available_vk14_features.stippledBresenhamLines)
                || (vk14_features.stippledSmoothLines && !available_vk14_features.stippledSmoothLines)
                || (vk14_features.vertexAttributeInstanceRateDivisor && !available_vk14_features.vertexAttributeInstanceRateDivisor)
                || (vk14_features.vertexAttributeInstanceRateZeroDivisor && !available_vk14_features.vertexAttributeInstanceRateZeroDivisor)
                || (vk14_features.indexTypeUint8 && !available_vk14_features.indexTypeUint8)
                || (vk14_features.dynamicRenderingLocalRead && !available_vk14_features.dynamicRenderingLocalRead)
                || (vk14_features.maintenance5 && !available_vk14_features.maintenance5)
                || (vk14_features.maintenance6 && !available_vk14_features.maintenance6)
                || (vk14_features.pipelineProtectedAccess && !available_vk14_features.pipelineProtectedAccess)
                || (vk14_features.pipelineRobustness && !available_vk14_features.pipelineRobustness)
                || (vk14_features.hostImageCopy && !available_vk14_features.hostImageCopy)
                || (vk14_features.pushDescriptor && !available_vk14_features.pushDescriptor);
            }
            all_required_features_supported = !any_feature_missing;
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

void create_thread_staging_objects(ThreadStagingObjects* staging_objects)
{
    // Create transfer command pool with flag for resetting command buffers allocated from it
    VkCommandPoolCreateInfo transfer_cmdpool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = renderstate.queue_family_indices.transfer_family
    };
    VK_CHECK(vkCreateCommandPool(renderstate.device, &transfer_cmdpool_create_info, NULL, &staging_objects->transfer_command_pool));

    // Create a command buffer we will use for all this thread's copy commands from staging buffers to dedicated ones.
    VkCommandBufferAllocateInfo upload_buf_alloc_info = {
        .sType               = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext               = NULL,
        .commandPool         = staging_objects->transfer_command_pool,
        .level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount  = 1
    };
    vkAllocateCommandBuffers(renderstate.device, &upload_buf_alloc_info, &staging_objects->upload_command_buffer);
}

void destroy_thread_staging_objects(ThreadStagingObjects* staging_objects)
{
    vkDestroyCommandPool(renderstate.device, staging_objects->transfer_command_pool, NULL);   
}

void thread_safe_submit_cmd(VkCommandBuffer cmd, VkFence fence)
{
    SDL_LockMutex(renderstate.transfer_queue_mutex);

    VkCommandBufferSubmitInfo cmd_submit_info = {
        .sType          = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext          = NULL,
        .commandBuffer  = cmd,
        .deviceMask     = 0
    };

    // Submit command buffer to the queue to execute it
    VkSubmitInfo2 submit_info = {
        .sType                     = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, 
        .pNext                     = NULL,
        .flags                     = 0,
        .waitSemaphoreInfoCount    = 0,
        .pWaitSemaphoreInfos       = NULL,
        .commandBufferInfoCount    = 1,
        .pCommandBufferInfos       = &cmd_submit_info,
        .signalSemaphoreInfoCount  = 0,
        .pSignalSemaphoreInfos     = NULL
    };
    VK_CHECK(vkQueueSubmit2(renderstate.transfer_queue, 1, &submit_info, fence));

    SDL_UnlockMutex(renderstate.transfer_queue_mutex);
}
