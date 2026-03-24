#include "engine.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>  // std::clamp, can easily remove this dependency

// NOTE(Liam): All global state here should be const.


#define API_VERSION VK_API_VERSION_1_4
const char* app_name = "brrrrrrrrrrrrrrrrrrrrrr";
const u32 app_version = VK_MAKE_VERSION(0, 0, 0);
const char* engine_name = "litehmrrrr";
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
vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    VulkanEngine* loaded_engine = (VulkanEngine*)pUserData;


    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        fprintf(stderr, ANSI_CYAN "Validation Layer Verbose: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        fprintf(stderr, ANSI_CYAN "Validation Layer Info: " ANSI_CYAN "%s\n" ANSI_RESET, pCallbackData->pMessage);
        break;
        
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:

        fprintf(stderr, ANSI_CYAN "Validation Layer %s: " ANSI_YELLOW "%s\n" ANSI_RESET,
            messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "Warning" : "Error",
            pCallbackData->pMessage);
        // *(volatile int*)0 = 0;  // Crash the program so I can backtrace
        loaded_engine->program_caused_vulkan_validation_layer_errors = 1;
        break;
        default: break;
    }

    return VK_FALSE;
}

void
vk_init(VulkanEngine* engine, VkExtent2D window_extents)
{
    // engine->engine_stage = ENGINE_STAGE_INIT;

    // Display current compiler settings being used
    #ifdef NDEBUG
    printf("[Release Mode] ");
    #else
    printf("[Debug Mode] ");
    #endif
    #ifdef ENABLE_VERBOSE_LOGGING
    printf("[Verbose Logging Enabled]\n");
    #else
    printf("[Verbose Logging Disabled]\n");
    #endif

    // Init the main memory tracker for the main thread which in debug mode we can query it for memory leaks during cleanup
    engine->main.tt = init_per_thread_allocation_tracker("MainThread_MainTracker");

    engine->render_mode = RENDERMODE_DEFAULT_DEFERRED;

    // Init video flags
    engine->frame_number = 0;
    engine->window_extents = window_extents;
    engine->window_minimized = 0;
    engine->window_resizing = 0;
    engine->uncapped_fps = 0;
    engine->program_caused_vulkan_validation_layer_errors = 0;

    engine->using_validation_layers = request_validation_layers;

    // Setup GLFW
    {
        if (!glfwInit())
        {
            fprintf(stderr, "Failed to init GLFW\n");
            exit(1);
        }

        if (glfwVulkanSupported() == GLFW_FALSE)
        {
            fprintf(stderr, "GLFW says Vulkan not supported\n");
            exit(1);
        }

        // NOTE(Liam): Fuck wayland automatic fractional scaling for now (it's dog water)
        // TODO: When readable UI becomes important: get the scaling factor via glfwGetMonitorContentScale
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // For Vulkan
        engine->window = glfwCreateWindow(engine->window_extents.width, engine->window_extents.height, "brrrrrr", NULL, NULL);
        if (!engine->window)
        {
            fprintf(stderr, "Failed to create GLFW window\n");
            exit(1);
        }
        glfwSetWindowSizeCallback(engine->window, glfw_window_size_callback);
        glfwSetKeyCallback(engine->window, glfw_key_callback);
        glfwSetCursorPosCallback(engine->window, glfw_mouse_motion_callback);
        glfwSetMouseButtonCallback(engine->window, glfw_mouse_button_callback);

        // Let GLFW callbacks can access our engine struct
        glfwSetWindowUserPointer(engine->window, engine);
    }
    
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
    VERBOSE_LOG("Vulkan loader version: %d.%d.%d (variant %d)\n",
        VK_API_VERSION_MAJOR(loader_api_version), VK_API_VERSION_MINOR(loader_api_version),
        VK_API_VERSION_PATCH(loader_api_version), VK_API_VERSION_VARIANT(loader_api_version)
    );
    
    // Enable validation layers
    if (engine->using_validation_layers)
    {
        u32 available_layer_count;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));
        
        VkLayerProperties* available_layers = (VkLayerProperties*)L_calloc(available_layer_count, sizeof(VkLayerProperties), &engine->main.tt);
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
            abort();
        }

        L_free(available_layers, &engine->main.tt);

        VERBOSE_LOG("Debug mode: Using validation layers.\n");
    }
    else
    {
        VERBOSE_LOG("Release mode: No validation layers.\n");
    }


    // Create instance
    {
        engine->instance = VK_NULL_HANDLE;


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

        u32 glfw_extensions_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);
        
        // Also include the debug utils extensions if using_validation_layers
        u32 instance_extensions_count = glfw_extensions_count + extra_instance_extensions_count + (engine->using_validation_layers ? 1 : 0);
        const char** instance_extensions = (const char**)L_calloc(sizeof(char*), instance_extensions_count, &engine->main.tt);
        for (u32 i = 0; i < glfw_extensions_count; ++i)
        {
            instance_extensions[i] = glfw_extensions[i];
        }
        for (u32 i = 0; i < extra_instance_extensions_count; ++i)
        {
            instance_extensions[glfw_extensions_count + i] = extra_instance_extensions[i];
        }
        if (engine->using_validation_layers)
        {
            instance_extensions[glfw_extensions_count + extra_instance_extensions_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        
        // Display extensions
        VERBOSE_LOG("Instance Extensions: ");
        if (instance_extensions_count)
        {
            for (u32 i = 0; i < instance_extensions_count-1; ++i)
                VERBOSE_LOG("%s, ", instance_extensions[i]);
            VERBOSE_LOG("%s\n", instance_extensions[instance_extensions_count-1]);
        }
        else
        {
            VERBOSE_LOG("None\n");
        }

        VkInstanceCreateInfo instance_create_info = {};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &app_info;
        instance_create_info.enabledExtensionCount = instance_extensions_count;
        instance_create_info.ppEnabledExtensionNames = instance_extensions;

        if (engine->using_validation_layers)
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


        VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &engine->instance));

        L_free(instance_extensions, &engine->main.tt);


        assert(engine->instance != VK_NULL_HANDLE);
    }

    // Get the rest of the Vulkan procs that our VkInstance requires using volk 
    volkLoadInstance(engine->instance);

    // Setup debug message callback
    if (engine->using_validation_layers)
    {
        engine->debug_messenger = VK_NULL_HANDLE;


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
        
        debug_messenger_create_info.pfnUserCallback = vk_debug_callback;
        debug_messenger_create_info.pUserData = engine;

        VK_CHECK(vkCreateDebugUtilsMessengerEXT(engine->instance, &debug_messenger_create_info, NULL, &engine->debug_messenger));


        assert(engine->debug_messenger != VK_NULL_HANDLE);
    }
    else
    {
        engine->debug_messenger = VK_NULL_HANDLE;
    }

    // Create a window surface
    {
        engine->surface = VK_NULL_HANDLE;


        VK_CHECK(glfwCreateWindowSurface(engine->instance, engine->window, NULL, &engine->surface));
        // NOTE: The VK_KHR_surface extension is part of the list from glfwGetRequiredInstanceExtensions
        // GLFW can handle the differences in vulkan surface creation between Windows and Linux

        VERBOSE_LOG("Created Window Surface\n");


        assert(engine->surface != VK_NULL_HANDLE);
    }
    // NOTE: We had to create the window surface directly after instance
    // creation because it influences physical device selection.

    // Choose a physical device
    {
        engine->physical_device = VK_NULL_HANDLE;


        u32 device_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(engine->instance, &device_count, NULL));
        if (device_count == 0)
        {
            fprintf(stderr, "Could not find a GPU with Vulkan support... Exiting");
            abort();
        }

        VkPhysicalDevice* devices = (VkPhysicalDevice*)L_calloc(device_count, sizeof(VkPhysicalDevice), &engine->main.tt);
        VK_CHECK(vkEnumeratePhysicalDevices(engine->instance, &device_count, devices));

        // Give each device a suitability score and pick the best one (e.g. dGPU > iGPU)
        int* device_suitability_score = (int*)L_calloc(device_count, sizeof(int), &engine->main.tt);
        for (u32 i = 0; i < device_count; ++i)
        {
            VERBOSE_LOG("Device %d:\n", i);
            device_suitability_score[i] = score_physical_device_and_check_required_features(engine, devices[i]);
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
        engine->physical_device = devices[candidate_device_index];
    
        // Store physical device properties
        vkGetPhysicalDeviceProperties(engine->physical_device, &engine->physical_device_properties);

        VERBOSE_LOG("Selected device %d.\n", candidate_device_index);
        VERBOSE_LOG("Device Extensions: ");
        if (device_extensions_count)
        {
            for (u32 i = 0; i < device_extensions_count-1; ++i)
                VERBOSE_LOG("%s, ", device_extensions[i]);
            VERBOSE_LOG("%s\n", device_extensions[device_extensions_count-1]);
        }
        else
        {
            VERBOSE_LOG("None\n");
        }

        L_free(device_suitability_score, &engine->main.tt);
        L_free(devices, &engine->main.tt);


        assert(engine->physical_device != VK_NULL_HANDLE);
    }

    // Find queue families of selected physical device
    {
        engine->queue_family_indices = get_physical_device_queue_family_indices(engine, engine->physical_device);
        
        VERBOSE_LOG("Graphics queue family at index %d\n", engine->queue_family_indices.graphics_family);
        VERBOSE_LOG("Presentation (surface) queue family at index %d\n", engine->queue_family_indices.present_family);


        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            assert(engine->queue_family_indices.array[i] > -1);
        }
    }

    // Create Logical Device
    {
        engine->device = VK_NULL_HANDLE;


        // Create the queues we will submit commands to
        VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)L_calloc(NUM_QUEUE_FAMILY_INDICES, sizeof(VkDeviceQueueCreateInfo), &engine->main.tt);
        float queue_priorities[] = { 1.0f };

        // Queues can be available for multiple queue families, so we make sure we only make the unique queues
        int num_unique_queues = 0;
        for (int i = 0; i < NUM_QUEUE_FAMILY_INDICES; ++i)
        {
            // See if this queue is unique so far
            b32 is_unique_queue = 1;
            for (int j = 0; j < i; ++j)
                if (engine->queue_family_indices.array[j] == engine->queue_family_indices.array[i])
                    is_unique_queue = 0;
            
            if (is_unique_queue)
            {
                VkDeviceQueueCreateInfo queue_create_info = {};
                queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_info.queueFamilyIndex = (u32)engine->queue_family_indices.array[i];
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
        
        VK_CHECK(vkCreateDevice(engine->physical_device, &device_create_info, NULL, &engine->device));

        if (!engine->using_validation_layers)
        {
            // Volk: Specialize Vulkan Functions for this device (optional)
            volkLoadDevice(engine->device);
            // NOTE: This volk functionality is not suitable for applications that want to use multiple VkDevice objects concurrently.
            // But it has a tiny performance improvement by having one less indirection by loading the device's specific functions directly.
        }

        // Get the queue handles from the device
        vkGetDeviceQueue(engine->device, engine->queue_family_indices.graphics_family, 0, &engine->graphics_queue);
        vkGetDeviceQueue(engine->device, engine->queue_family_indices.present_family, 0, &engine->presentation_queue);

        L_free(queue_create_infos, &engine->main.tt);

        VERBOSE_LOG("Created Logcal Device.\n");


        assert(engine->device != VK_NULL_HANDLE);
    }

    // Init Vulkan Memory Allocator
    {
        engine->vma_allocator = VK_NULL_HANDLE;


        VmaAllocatorCreateInfo allocator_create_info = {};
        allocator_create_info.physicalDevice = engine->physical_device;
        allocator_create_info.device = engine->device;
        allocator_create_info.instance = engine->instance;
        allocator_create_info.flags =
            VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT  // For GPU pointers
        ;
  
        VmaVulkanFunctions vulkan_functions;
        VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocator_create_info, &vulkan_functions));
  
        allocator_create_info.pVulkanFunctions = &vulkan_functions;

        VK_CHECK(vmaCreateAllocator(&allocator_create_info, &engine->vma_allocator));
  
        VERBOSE_LOG("Vulkan Memory Allocator Initialised\n");


        assert(engine->vma_allocator != VK_NULL_HANDLE);
    }

    // Init pool and command staging buffers use to perform a one time copy command in a more efficent way than creating a whole command pool each time
    {
        // Transfer pool:
        VkCommandPoolCreateInfo onetime_cmdpool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            // NOTE: The transient flag bit tells the driver the commands allocated from this pool will be short lived and frequently recorded and reset.
            .queueFamilyIndex = (u32)engine->queue_family_indices.graphics_family
        };

        engine->onetime_command_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(engine->device, &onetime_cmdpool_create_info, NULL, &engine->onetime_command_pool));

        // Transfer command:
        engine->onetime_command = vklayer_alloc_cmd_buffer(engine->device, engine->onetime_command_pool);

        // Transfer fence:
        VkFenceCreateInfo onetime_fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };
        VK_CHECK(vkCreateFence(engine->device, &onetime_fence_create_info, NULL, &engine->onetime_command_complete_fence));

        VERBOSE_LOG("Created Transfer Command Pool for staging buffers\n");
    }

    // Create default sampler
    {
        engine->use_anisotropic_filtering = 0;

        VkSamplerCreateInfo sampler_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .magFilter         = VK_FILTER_LINEAR,
            .minFilter         = VK_FILTER_LINEAR,
            .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_LINEAR,  // aka trilinear filtering
            .addressModeU      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW      = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias        = 0.0f,  // Bias default=0, positive val shifts towards lower resolution mipmap. Negative -> towards higher res.
            .anisotropyEnable  = engine->use_anisotropic_filtering ? VK_TRUE : VK_FALSE,
            .maxAnisotropy     = engine->physical_device_properties.limits.maxSamplerAnisotropy,  // Set to devices max anisotropy (TODO: Haven't thought too hard about that decision to be honest)
            .compareEnable     = VK_FALSE,
            .compareOp         = VK_COMPARE_OP_NEVER,
            .minLod            = 0.0f,
            .maxLod            = VK_LOD_CLAMP_NONE,
            .borderColor       = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // Doesn't apply with repeat addressing mode.
            .unnormalizedCoordinates = VK_FALSE
        };

        engine->default_sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(engine->device, &sampler_create_info, NULL, &engine->default_sampler));
        

        VERBOSE_LOG("Created Default Sampler");
        if (engine->use_anisotropic_filtering) VERBOSE_LOG(" with max anisotropy of %f", sampler_create_info.maxAnisotropy);
        else VERBOSE_LOG(" with anisotropic filtering disabled!");
        VERBOSE_LOG("\n");

        assert(engine->default_sampler != VK_NULL_HANDLE);
    }

    // Create all descriptor set layouts
    // NOTE: Must be done before graphics pipeline creation
    create_all_descriptor_set_layouts(engine);
    
    // Create SwapChain which also creates the Graphics Pipelines
    {
        engine->swapchain = VK_NULL_HANDLE;
        engine->is_a_swapchain_created = 0;
        create_or_recreate_swapchain(engine);
        
        assert(engine->is_a_swapchain_created);
        assert(engine->swapchain != VK_NULL_HANDLE);
    }

    // Init FrameData per frame overlap (multiple frames so that we can always access a resource immedietely)
    {
        VkCommandPoolCreateInfo cmdpool_create_info = {};
        cmdpool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdpool_create_info.pNext = NULL;
        cmdpool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // NOTE: We could instead rely on resetting the whole command pool
        cmdpool_create_info.queueFamilyIndex = engine->queue_family_indices.graphics_family;

        for (int i = 0; i < FRAME_OVERLAP; ++i)
        {
            VK_CHECK(vkCreateCommandPool(engine->device, &cmdpool_create_info, NULL, &engine->frames[i].command_pool));

            // Allocate main command buffer we'll use for rendering from the pool we just created
            engine->frames[i].main_command_buffer = vklayer_alloc_cmd_buffer(engine->device, engine->frames[i].command_pool);
        }


        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.pNext = NULL;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < FRAME_OVERLAP; ++i)
        {
            // Init sync structures
            VK_CHECK(vkCreateFence(engine->device, &fence_create_info, NULL, &engine->frames[i].render_fence));
            
            // Create lights buffer
            engine->frames[i].lights_buffer = create_buffer(
                engine->vma_allocator,
                LIGHT_BUFFER_SIZE,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU
            );
            // NOTE: Enforcing sequential writing to the buffer because I want to assume all lights are dynamic.
            // We want consistant framerate, so we must optimize for writing all the lights every frame anyway.

            // Create the lights descriptor and make it point to the lights buffer we just made
            assert(engine->lights_set_layout != VK_NULL_HANDLE);
            engine->frames[i].lights_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->lights_set_layout);
            {
                // Identify the buffer
                VkDescriptorBufferInfo descriptor_buffer_info = {
                    .buffer = engine->frames[i].lights_buffer.buffer,
                    .offset = 0,
                    .range  = LIGHT_BUFFER_SIZE
                };

                VkWriteDescriptorSet descriptor_write = {};
                descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_write.pNext = NULL;
                descriptor_write.dstSet = engine->frames[i].lights_descriptor_set;
                descriptor_write.dstBinding = LIGHTS_DESCRIPTOR_SET_BINDING;
                descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor_write.descriptorCount = 1;
                descriptor_write.pBufferInfo = &descriptor_buffer_info;

                vkUpdateDescriptorSets(engine->device, 1, &descriptor_write, 0, NULL);
            }
        }

        VERBOSE_LOG("Per frame command buffers created (cycling through %d frames in flight)\n", FRAME_OVERLAP);
    }
    
    // Create draw lists (TODO: Will happen on scene load if this engine expands)
    create_draw_lists(engine, &engine->draw_lists);
    engine->clear_color = glm::vec4(0.392f, 0.584f, 0.929f, 1.0f);  // Cornflower blue because XNA used to be cool.

    // Zero the scene struct. main.cpp or elsewhere should initialize it for now TODO: Refactor the scene API to reduce future technical debt, but for now it gets this sun temple working fine.
    engine->scene = {};
    engine->scene.is_initialised = 0;

    engine->do_bloom = 1;
    engine->do_postprocessing = 0;

    VERBOSE_LOG("\n*********** Finished Init ***********\n\n");
}

void
vk_cleanup(VulkanEngine* engine)
{
    printf("\n*********** Clean Up ***********\n");

    // Ensure device is not doing anywork before destroying it's stuff.
    vkDeviceWaitIdle(engine->device);
    
    // Destroy per-frame sync objects
    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        vkDestroyCommandPool(engine->device, engine->frames[i].command_pool, NULL);
        vkDestroyFence(engine->device, engine->frames[i].render_fence, NULL);
    }

    //
    // Destroy global objects
    //

    destroy_draw_lists(engine, &engine->draw_lists);
    destroy_all_graphics_pipelines(engine);
    destroy_swapchain(engine, engine->swapchain);
    destroy_all_descriptor_set_layouts(engine);
    vkDestroySampler(engine->device, engine->default_sampler, NULL);      // Destroy default sampler
    destroy_scene(engine);

    // Destroy one time command stuff:
    vkDestroyFence(engine->device, engine->onetime_command_complete_fence, NULL);
    vkDestroyCommandPool(engine->device, engine->onetime_command_pool, NULL);

    // Destroy fundamental Vulkan objects
    vmaDestroyAllocator(engine->vma_allocator);
    vkDestroyDevice(engine->device, NULL);
    vkDestroySurfaceKHR(engine->instance, engine->surface, NULL);
    if (engine->using_validation_layers)
    {
        vkDestroyDebugUtilsMessengerEXT(engine->instance, engine->debug_messenger, NULL);
    }
    vkDestroyInstance(engine->instance, NULL);


    // Destroy Window
    glfwDestroyWindow(engine->window);
    

    // Display if we had correct Vulkan API usage
    if (engine->using_validation_layers)
    {
        if (!engine->program_caused_vulkan_validation_layer_errors)
        {
            // Vulkan clean up successful
            VERBOSE_LOG(ANSI_CYAN "Vulkan cleanup was successful: No complaints from the enabled validation layers.\n" ANSI_RESET);
        }
        else
        {
            VERBOSE_LOG(ANSI_MAGENTA "Vulkan validation layer errors/warnings appeared :(\n" ANSI_RESET);
        }
    }


    // Report memory leaks of main tracker
    VERBOSE_LOG("\n***********  Memory Tracker Results ***********\n");
    check_tracker_for_memory_leaks(&engine->main.tt);
}

void
vk_tick_game(VulkanEngine* engine, float dt)
{
    static b32 game_state_initialized = 0;
    if (!game_state_initialized)
    {
        // INIT GAME STATE
        engine->camera_fov = 70.0f;
        engine->camera_position = glm::vec3(0.0f, -0.5f, 5.0f);
        engine->camera_pitch = 0.0f;
        engine->camera_yaw = 0.0f;
        
        game_state_initialized = 1;
    }


    // Look with camera
    // NOTE: We only do this 2 frames after mouse being capture so that the mouse positions
    // don't make a massive jump when it initially gets locked to the centre.
    if (engine->input.mouse_is_captured && engine->last_input.mouse_is_captured)
    {
        const float camera_mouse_sensitivity = 0.01f;  // radians per second

        // Get change in mouse position and multiply by mouse sensitivity
        double mouse_dx = engine->input.mouse_xpos - engine->last_input.mouse_xpos;
        double mouse_dy = engine->input.mouse_ypos - engine->last_input.mouse_ypos;
        mouse_dx *= camera_mouse_sensitivity;
        mouse_dy *= camera_mouse_sensitivity;

        engine->camera_yaw   += mouse_dx;
        engine->camera_pitch += mouse_dy;

        // Clamp pitch to avoid going upside-down
        if (engine->camera_pitch < -M_PI/2.0f)     engine->camera_pitch = -M_PI/2.0f;
        else if (engine->camera_pitch > M_PI/2.0f) engine->camera_pitch = M_PI/2.0f;

        // Wrap yaw to [0, 2PI)
        if (engine->camera_yaw < 0.0f) engine->camera_yaw += 2.0f * M_PI;
        engine->camera_yaw = fmodf(engine->camera_yaw, 2.0f * M_PI);
    }


    // Move camera
    float speed = 6.0f;
    if (engine->input.key_move_faster)      speed *= 5.0f;
    else if (engine->input.key_move_slower) speed *= 0.05f;

    
    float x = (float)(engine->input.key_right - engine->input.key_left);
    float y = (float)(engine->input.key_up    - engine->input.key_down);
    float z = (float)(engine->input.key_forward  - engine->input.key_back);

    // Normalize if going along more than one axis
    float len_squared = x*x + z*z;
    if (len_squared > 0)
    {
        speed /= sqrt(len_squared);
    }

    float dx = speed * dt * sinf(engine->camera_yaw);
    float dz = speed * dt * cosf(engine->camera_yaw);
    engine->camera_position.x += dx * z + dz * x;
    engine->camera_position.z += dx * x - dz * z;

    engine->camera_position.y += y * speed * dt;
    

    // glm::mat4 rot = glm::rotate(glm::mat4(1.0f), engine->camera_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    // rot = glm::rotate(rot, engine->camera_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::vec4 right   = rot * glm::vec4(1.0f, 0.0f,  0.0f, 0.0f);
    // glm::vec4 up      = rot * glm::vec4(0.0f, 1.0f,  0.0f, 0.0f);
    // glm::vec4 forward = rot * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

    // float rightward_movement = speed * dt * x;
    // float upward_movement    = speed * dt * y;
    // float forward_movement   = speed * dt * z;

    // engine->camera_position.x += rightward_movement*right.x + upward_movement*up.x + forward_movement*forward.x;
    // engine->camera_position.y += rightward_movement*right.y + upward_movement*up.y + forward_movement*forward.y;
    // engine->camera_position.z += rightward_movement*right.z + upward_movement*up.z + forward_movement*forward.z;
}

void
vk_draw(VulkanEngine* engine)
{
    clear_draw_lists(&engine->draw_lists);
    
    if (engine->scene.is_initialised)
    {
        // Draw statics in scene
        for (u32 i = 0; i < engine->scene.statics_size; ++i)
        {
            Renderable* renderable_ref = &engine->scene.renderables[engine->scene.static_renderable_ids[i]];
            RenderpassIndex renderpass_id = renderable_ref->is_opaque ?
                RENDERPASS_INDEX_RENDERPASS_OPAQUE : RENDERPASS_INDEX_RENDERPASS_TRANSPARENT;

            DrawCommandInfo static_draw_info = {
                .renderable_ref = renderable_ref,
                .model_matrix = glm::mat4(1.0f)
            };
            
            draw_to_graphics_pipeline(engine, renderpass_id, static_draw_info);
        }
    }

    vk_dispatch_draws(engine);
}

void
vk_dispatch_draws(VulkanEngine* engine)
{
    // Get which frame is in flight (e.g. which command buffers are ok to write to)
    engine->current_frame_id = engine->frame_number % FRAME_OVERLAP;
    FrameData* current_frame = &engine->frames[engine->current_frame_id];


    // Wait for fences, don't reset the fence until we know we aren't going to recreate the swapchain
    // because in that case we exit early from vk_draw and then when vk_draw is called again the fence would be
    // set already and cause the program to wait forever.
    VK_CHECK(vkWaitForFences(engine->device, 1, &current_frame->render_fence, VK_TRUE, SYNC_TIMEOUT_NANOSECONDS));

    u32 image_acquired_semaphore_id = engine->frame_number % engine->swapchain_image_count;

    // Get next swapchain image.
    // NOTE: If another frame isn't finished with the swapchain image, the GPU must't execute commands on the next swapchain image.
    //       so it must have attained the image_aquired semaphore first.
    u32 swapchain_image_index;
    VkResult acquire_result = vkAcquireNextImageKHR(engine->device, engine->swapchain, SYNC_TIMEOUT_NANOSECONDS, engine->swapchain_image_acquired_semaphores[image_acquired_semaphore_id], VK_NULL_HANDLE, &swapchain_image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR)
    {
        // NOTE: This handles resizes explicitly for drivers that don't explicitly trigger
        // VK_ERROR_OUT_OF_DATE_KHR automatically on window resize (hence not triggering the glfw callback)
        // I don't get this issue on my laptop but it seems like it may be needed for some machines.

        // Return from draw function, marking the window to be resized and subtracting the framenumber
        // since we aren't drawing until the next main loop cycle.
        engine->window_resizing = 1;
        --engine->frame_number;
        return;
    }
    VK_CHECK(acquire_result);

    
    // Now we can safely reset the render fence.
    // NOTE: The render fence is in place to make sure this frame's graphics commands have finished executing on the GPU.
    VK_CHECK(vkResetFences(engine->device, 1, &current_frame->render_fence));


    // Reset this frame-data's command buffer, now we know that commands have finished executing
    VkCommandBuffer cmd = current_frame->main_command_buffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));  // Removes all commands and frees their memory



    // Setup command buffer for recording
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext = NULL;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We will only submit it once before resetting it
    cmd_buffer_begin_info.pInheritanceInfo = NULL;


    //
    // BEGIN COMMAND BUFFER RECORDING
    //

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_buffer_begin_info));
    {

        // Put render target images into initial formats
        VkImageMemoryBarrier2 initial_image_barriers[] = {

            // Color attachment
            vklayer_specify_image_transition_barrier(
                engine->render_target_image.image,                // The image
                vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),  // The aspect(s), miplevels, and array layers of the image that the barrier applies to
                /* Before */
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,             // The latest stage where previous commands must finish
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,           // What operations on what resource must be made visible before the transition
                engine->render_target_image.current_layout,       // Current image format
                VK_QUEUE_FAMILY_IGNORED,                          // Queue family that current only the image  (use ignored when not transferring ownership)
                /* After */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,  // The earliest stage where following commands may start using the results
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,           // What operations on what resource require this transition first
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,         // Resulting image format
                VK_QUEUE_FAMILY_IGNORED                           // Queue family that will own the image after the barrier (use ignored when not transferring ownership)

                // As for the queues ownership transfers, the queues in hardware may represent different hardware blocks without memory coherency between them.
                // E.g. a graphics queue may run on a unified shader core,
                // a compute queue may run on a compute-only core.
                // a transfer queue may run on a dedicated DMA engine (Direct Memory Access).
                // These hardware blocks often cannot read/right GPU memory coherently, they will likely have seperate cache hierarchies.
                // Something must explicitly transfer ownership before another hardware unit touches the resource.
            ),

            // Depth stencil attachment
            vklayer_specify_image_transition_barrier(
                engine->render_target_depth.image,
                vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
                /* Before */
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                engine->render_target_depth.current_layout,
                VK_QUEUE_FAMILY_IGNORED,
                /* After */
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED
            )
        };
        GPU_Image* initial_image_barrier_images[] = {
            &engine->render_target_image,
            &engine->render_target_depth
        };
        transition_gpu_images(cmd, sizeof(initial_image_barriers)/sizeof(initial_image_barriers[0]), initial_image_barriers, initial_image_barrier_images);


        //
        // COPY LIGHTS TO SHADER STORAGE BUFFER
        //

        copy_lights_to_gpu(engine, current_frame);


        //
        // RENDER SHADOW MAP
        //
        {
            // Transition shadow map
            VkImageMemoryBarrier2 shadowmap_image_barriers[] = {
                vklayer_specify_image_transition_barrier(
                    engine->shadow_map_depth.image,
                    vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    engine->shadow_map_depth.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* shadowmap_image_barrier_images[] = {
                &engine->shadow_map_depth
            };
            transition_gpu_images(cmd, sizeof(shadowmap_image_barriers) / sizeof(shadowmap_image_barriers[0]), shadowmap_image_barriers, shadowmap_image_barrier_images);

            // Directional shadow map for the Sun
            update_scene_uniforms(engine, cmd, calculate_shadow_uniforms(engine));

            //
            // SETUP RENDERPASS: Shadow map
            //

            // TODO: Lots of shared code in setting up renderpasses (such as below):
            // Creating a rendergraph system would be very worth while

            VkExtent2D render_extent = { engine->shadow_map_depth.image_extent.width, engine->shadow_map_depth.image_extent.height };
            const VkRect2D render_area = {
                .offset={ 0, 0 },
                .extent=render_extent
            };

            const VkViewport viewport = {
                .x         = 0.0f,
                .y         = 0.0f,
                .width     = (float)engine->shadow_map_depth.image_extent.width,
                .height    = (float)engine->shadow_map_depth.image_extent.height,
                .minDepth  = 0.0f,
                .maxDepth  = 1.0f
            };
            const VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = render_extent
            };

            VkRenderingAttachmentInfo depth_attachment = {
                .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext               = NULL,
                .imageView           = engine->shadow_map_depth.image_view,
                .imageLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

                // No multisampling
                .resolveMode         = VK_RESOLVE_MODE_NONE,
                .resolveImageView    = VK_NULL_HANDLE,
                .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                .loadOp              = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                
                // Clear depth to 1.0f (Far plane in NDCs)
                // TODO: If changing to reverse Z depth values, make sure this is up-to-date.
                .clearValue          = {
                    .depthStencil = {
                        .depth = 1.0f,
                        .stencil = 0
                    }
                }
            };

            // Specify attachments for this graphics pipeline's renderpass
            RenderingAttachments rendering_attachments = {
                .color_attachment_count  = 0,
                .color_attachments       = NULL,
                .color_attachment_images = NULL,
                .depth_attachment        = &depth_attachment,
                .depth_attachment_image  = &engine->shadow_map_depth.image,

                .render_area             = render_area,
                .viewport                = viewport,
                .scissor                 = scissor
            };

            // Opaque pass
            record_renderpass(engine, cmd, current_frame, false, RENDERPASS_INDEX_RENDERPASS_OPAQUE, &engine->gp_shadowmap_opaque, rendering_attachments);
            
            // Transparent pass must wait for opaque pass to finish writing to the depth buffer
            {
                // Transition shadow map:
                VkImageMemoryBarrier2 opaque_depth_writes_finished_barrier = vklayer_specify_image_transition_barrier(
                    engine->shadow_map_depth.image,
                    vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    engine->shadow_map_depth.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                );
                GPU_Image* depth_gpu_image = &engine->shadow_map_depth;
                transition_gpu_images(cmd, 1, &opaque_depth_writes_finished_barrier, &depth_gpu_image);
            }

            // Transparent pass
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            record_renderpass(engine, cmd, current_frame, false, RENDERPASS_INDEX_RENDERPASS_TRANSPARENT, &engine->gp_shadowmap_transparent, rendering_attachments);
        }

        //
        // TRANSITION SHADOW MAPS SO WE CAN SAMPLE THEM AS TEXTURES
        //
        {
            // Transition depth to read&write state
            VkImageMemoryBarrier2 depth_barriers[] = {
                vklayer_specify_image_transition_barrier(
                    engine->shadow_map_depth.image,
                    vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    engine->shadow_map_depth.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* depth_barrier_images[] = {
                &engine->shadow_map_depth
            };
            transition_gpu_images(cmd, sizeof(depth_barriers)/sizeof(depth_barriers[0]), depth_barriers, depth_barrier_images);
        }

        //
        // UPDATE SCENE UNIFORMS FOR PLAYER CAMERA PERSPECTIVE
        //

        // Update buffer with current scene camera information
        SceneUniform_GLSL_ScalarBlock scene_uniform_data = calculate_scene_uniforms(
            engine, engine->render_target_extent.width, engine->render_target_extent.height
        );
        update_scene_uniforms(engine, cmd, scene_uniform_data);


        //
        // Common viewport and scissor rectangle for render passes
        //

        const VkRect2D render_area = {
            .offset={ 0, 0 },
            .extent=engine->render_target_extent
        };

        const VkViewport viewport = {
            .x         = 0.0f,
            .y         = 0.0f,
            .width     = (float)engine->render_target_extent.width,
            .height    = (float)engine->render_target_extent.height,
            .minDepth  = 0.0f,
            .maxDepth  = 1.0f
        };
        const VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = engine->render_target_extent
        };
        

        if (engine->render_mode == RENDERMODE_DEFAULT_DEFERRED)
        {
            // Transition G-Buffer images to a writeable state
            VkImageMemoryBarrier2 gbuffer_write_barriers[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
            GPU_Image*            gbuffer_write_barrier_images[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
            for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
            {
                GPU_Image* attachment = &engine->render_target_deferred_attachments[i];
                gbuffer_write_barriers[i] = vklayer_specify_image_transition_barrier(
                    attachment->image,
                    vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    attachment->current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                );

                gbuffer_write_barrier_images[i] = attachment;
            }
            transition_gpu_images(cmd, sizeof(gbuffer_write_barriers)/sizeof(gbuffer_write_barriers[0]), gbuffer_write_barriers, gbuffer_write_barrier_images);

            //
            // SETUP RENDER PASS: Deferred geometry pass
            //
            {
                // G-Buffer color attachments
                VkImage color_images[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
                VkRenderingAttachmentInfo color_attachments[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
                for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
                {
                    color_images[i] = engine->render_target_deferred_attachments[i].image;
                    color_attachments[i] = {
                        .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .pNext               = NULL,
                        .imageView           = engine->render_target_deferred_attachments[i].image_view,
                        .imageLayout         = engine->render_target_deferred_attachments[i].current_layout,
                        
                        // No multisampling
                        .resolveMode         = VK_RESOLVE_MODE_NONE,
                        .resolveImageView    = VK_NULL_HANDLE,
                        .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                        .loadOp              = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue          = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } }
                    };
                }
                
                // Depth attachment
                VkImage depth_image = engine->render_target_depth.image;
                VkRenderingAttachmentInfo depth_attachment = {
                    .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext               = NULL,
                    .imageView           = engine->render_target_depth.image_view,
                    .imageLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

                    // No multisampling
                    .resolveMode         = VK_RESOLVE_MODE_NONE,
                    .resolveImageView    = VK_NULL_HANDLE,
                    .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                    .loadOp              = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                    
                    // Clear depth to 1.0f (Far plane in NDCs)
                    // TODO: If changing to reverse Z depth values, make sure this is up-to-date.
                    .clearValue          = {
                        .depthStencil = {
                            .depth = 1.0f,
                            .stencil = 0
                        }
                    }
                };

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = sizeof(color_attachments) / sizeof(VkRenderingAttachmentInfo),
                    .color_attachments       = color_attachments,
                    .color_attachment_images = color_images,
                    .depth_attachment        = &depth_attachment,
                    .depth_attachment_image  = &depth_image,

                    .render_area             = render_area,
                    .viewport                = viewport,
                    .scissor                 = scissor
                };

                // Draw objects listed for this graphics pipeline
                record_renderpass(engine, cmd, current_frame, false, RENDERPASS_INDEX_RENDERPASS_OPAQUE, &engine->gp_opaque_pass, rendering_attachments);
            }


            // Transition G-Buffer images to SHADER_READ_ONLY_OPTIMAL and depth to read&write state
            VkImageMemoryBarrier2 gbuffer_read_barriers[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
            GPU_Image*      gbuffer_read_barrier_images[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
            for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
            {
                // Color Attachments:
                GPU_Image* attachment = &engine->render_target_deferred_attachments[i];
                gbuffer_read_barriers[i] = vklayer_specify_image_transition_barrier(
                    attachment->image,
                    vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    attachment->current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                );

                gbuffer_read_barrier_images[i] = attachment;
            }
            // Depth Attachment:
            gbuffer_read_barriers[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = vklayer_specify_image_transition_barrier(
                engine->render_target_depth.image,
                vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
                /* Before */
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                engine->render_target_depth.current_layout,
                VK_QUEUE_FAMILY_IGNORED,
                /* After */
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED
            );
            gbuffer_read_barrier_images[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = &engine->render_target_depth;
            transition_gpu_images(cmd, sizeof(gbuffer_read_barriers)/sizeof(gbuffer_read_barriers[0]), gbuffer_read_barriers, gbuffer_read_barrier_images);


            //
            // Setup Deferred Lighting Pass
            //

            {
                GraphicsPipeline* gp = &engine->gp_deferred_lighting;

                // Bind descriptor set for Scene
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    gp->layout,
                    SCENE_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->scene_descriptor_set,
                    0,
                    NULL
                );


                // Bind descriptor set for G Buffers
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    gp->layout,
                    GBUFFERS_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->gbuffers_descriptor_set,
                    0,
                    NULL
                );

                // Bind descriptor set for the lights
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    gp->layout,
                    LIGHTS_DESCRIPTOR_SET_INDEX,
                    1,
                    &current_frame->lights_descriptor_set,
                    0,
                    NULL
                );

                // Bind descriptor set for the shadow map
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    gp->layout,
                    SHADOW_MAPS_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->shadow_maps_descriptor_set,
                    0,
                    NULL
                );

                VkRenderingAttachmentInfo color_attachments[1] = {};  // Output to the render target image

                // Color attachment (Clear whatever was in the buffer before)
                color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                color_attachments[0].imageView   = engine->render_target_image.image_view;
                color_attachments[0].imageLayout = engine->render_target_image.current_layout;
                color_attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color_attachments[0].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
                // NOTE: The clear colour never shows up in deferred because we overwrite it with a fullscreen triangle
                // We need to set the clear colour in the lighting fragment shader

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = sizeof(color_attachments) / sizeof(VkRenderingAttachmentInfo),
                    .pColorAttachments     = color_attachments,
                    .pDepthAttachment      = NULL,
                    .pStencilAttachment    = NULL
                };
                
                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->pipeline);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }
        }
        else
        {
            //
            // SETUP RENDER PASS: Forward+Opaque
            //

            // Color attachment (Clear whatever was in the buffer before)
            VkImage color_images[1] = { engine->render_target_image.image };
            VkRenderingAttachmentInfo color_attachments[1] = {};
            color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color_attachments[0].imageView   = engine->render_target_image.image_view;
            color_attachments[0].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;  // Note this must match the image barrier above
            color_attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachments[0].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachments[0].clearValue.color.float32[0] = engine->clear_color.r;
            color_attachments[0].clearValue.color.float32[1] = engine->clear_color.g;
            color_attachments[0].clearValue.color.float32[2] = engine->clear_color.b;
            color_attachments[0].clearValue.color.float32[3] = engine->clear_color.a;

            // Depth attachment
            VkImage depth_image = engine->render_target_depth.image;
            VkRenderingAttachmentInfo depth_attachment = {
                .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext               = NULL,
                .imageView           = engine->render_target_depth.image_view,
                .imageLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

                // No multisampling
                .resolveMode         = VK_RESOLVE_MODE_NONE,
                .resolveImageView    = VK_NULL_HANDLE,
                .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                .loadOp              = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                
                // Clear depth to 1.0f (Far plane in NDCs)
                // TODO: If changing to reverse Z depth values, make sure this is up-to-date.
                .clearValue          = {
                    .depthStencil = {
                        .depth = 1.0f,
                        .stencil = 0
                    }
                }
            };

            // Specify attachments for this graphics pipeline's renderpass
            RenderingAttachments forward_opaque_rendering_attachments = {
                .color_attachment_count  = sizeof(color_attachments) / sizeof(VkRenderingAttachmentInfo),
                .color_attachments       = color_attachments,
                .color_attachment_images = color_images,
                .depth_attachment        = &depth_attachment,
                .depth_attachment_image  = &depth_image,

                .render_area             = render_area,
                .viewport                = viewport,
                .scissor                 = scissor
            };

            // Draw objects listed for this graphics pipeline
            record_renderpass(engine, cmd, current_frame, true, RENDERPASS_INDEX_RENDERPASS_OPAQUE, &engine->gp_opaque_pass, forward_opaque_rendering_attachments);
        }

        //
        // Render transparents (alpha masked or alpha blended)
        //

        // Transparent pass must wait for opaque pass to finish writing to the depth buffer
        //
        // Transition Depth Buffer:
        VkImageMemoryBarrier2 opaque_depth_writes_finished_barrier = vklayer_specify_image_transition_barrier(
            engine->render_target_depth.image,
            vklayer_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT),
            /* Before */
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            engine->render_target_depth.current_layout,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        GPU_Image* depth_gpu_image = &engine->render_target_depth;
        transition_gpu_images(cmd, 1, &opaque_depth_writes_finished_barrier, &depth_gpu_image);


        //
        // SETUP RENDER PASS: Forward+Transparent
        //
        
        {
            // Color attachment (Transparent: Load the value already there instead of clearing)
            VkImage color_images[1] = { engine->render_target_image.image };
            VkRenderingAttachmentInfo color_attachments[1] = {};
            color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color_attachments[0].imageView   = engine->render_target_image.image_view;
            color_attachments[0].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;  // Note this must match the image barrier above
            color_attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_attachments[0].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            // NOTE: Clear value is ignored with LOAD_OP_LOAD

            // Depth attachment (Load existing depth value)
            VkImage depth_image = engine->render_target_depth.image;
            VkRenderingAttachmentInfo depth_attachment = {
                .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext               = NULL,
                .imageView           = engine->render_target_depth.image_view,
                .imageLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

                // No multisampling
                .resolveMode         = VK_RESOLVE_MODE_NONE,
                .resolveImageView    = VK_NULL_HANDLE,
                .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,

                .loadOp              = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                
                .clearValue={}  // NOTE: Ignored with LOAD_OP_LOAD
            };

            // Specify attachments for this graphics pipeline's renderpass
            RenderingAttachments forward_transparent_rendering_attachments = {
                .color_attachment_count  = sizeof(color_attachments) / sizeof(VkRenderingAttachmentInfo),
                .color_attachments       = color_attachments,
                .color_attachment_images = color_images,
                .depth_attachment        = &depth_attachment,
                .depth_attachment_image  = &depth_image,

                .render_area             = render_area,
                .viewport                = viewport,
                .scissor                 = scissor
            };

            // Draw objects listed for this graphics pipeline
            record_renderpass(engine, cmd, current_frame, true, RENDERPASS_INDEX_RENDERPASS_TRANSPARENT, &engine->gp_transparent_pass, forward_transparent_rendering_attachments);
        }


        //
        // BLOOM
        //

        if (engine->do_bloom)
        {
            //
            // Brightness extraction pass:
            //
            
            // Transition render_target_image to a shader readable format and use it to extract brightness and write it to the bloom attachment
            VkImageMemoryBarrier2 bloom_sample_render_target_barriers[2] = {
                vklayer_specify_image_transition_barrier(
                    engine->render_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    engine->render_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->bloom_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    engine->bloom_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* bloom_sample_render_target_barrier_images[2] = {
                &engine->render_target_image,
                &engine->bloom_target_image
            };
            transition_gpu_images(cmd, 2, bloom_sample_render_target_barriers, bloom_sample_render_target_barrier_images);
            
            // Write to the bloom brightness image
            // NOTE: Currently this pass is based on the other postprocess graphics pipelines.
            // TODO: At some point I want to make a render graph system so all these features aren't filled with so much code.
            const VkRect2D bloom_render_area = {
                .offset={ 0, 0 },
                .extent=engine->bloom_target_extent
            };
            const VkViewport bloom_viewport = {
                .x         = 0.0f,
                .y         = 0.0f,
                .width     = (float)engine->bloom_target_extent.width,
                .height    = (float)engine->bloom_target_extent.height,
                .minDepth  = 0.0f,
                .maxDepth  = 1.0f
            };
            const VkRect2D bloom_scissor = {
                .offset = { 0, 0 },
                .extent = engine->bloom_target_extent
            };

            {
                // Bind descriptor set for Scene
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_brightness_extraction.layout,
                    SCENE_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->scene_descriptor_set,
                    0,
                    NULL
                );

                // Bind image to postprocess
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_brightness_extraction.layout,
                    POSTPROCESS_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->postprocess_descriptor_set,
                    0,
                    NULL
                );
                // Color attachment
                VkRenderingAttachmentInfo color_attachment = {
                    .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext               = NULL,
                    .imageView           = engine->bloom_target_image.image_view,
                    .imageLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // Note this must match the image barrier above
                    .resolveMode         = VK_RESOLVE_MODE_NONE,
                    .resolveImageView    = VK_NULL_HANDLE,
                    .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp              = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue          = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } }
                };

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = 1,
                    .color_attachments       = &color_attachment,
                    .color_attachment_images = &engine->bloom_target_image.image,
                    .depth_attachment        = NULL,
                    .depth_attachment_image  = NULL,

                    .render_area             = bloom_render_area,
                    .viewport                = bloom_viewport,
                    .scissor                 = bloom_scissor
                };

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = rendering_attachments.render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = rendering_attachments.color_attachment_count,
                    .pColorAttachments     = rendering_attachments.color_attachments,
                    .pDepthAttachment      = rendering_attachments.depth_attachment,
                    .pStencilAttachment    = NULL  // TODO: Include stencil attachment
                };

                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->gp_bloom_brightness_extraction.pipeline);

                    // Dynamic pipeline states: Set viewport and scissor.
                    vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
                    vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }

            // Brightness is now stored in the lower res bloom_target_image
            //
            // Transition the brightness texture to a samplable state and horizontal blur it to the second bloom image
            VkImageMemoryBarrier2 bloom_prepare_horizontal_blur_barriers[2] = {
                vklayer_specify_image_transition_barrier(
                    engine->bloom_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    engine->bloom_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->bloom_pingpong_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    engine->bloom_pingpong_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* bloom_prepare_horizontal_blur_barrier_images[2] = {
                &engine->bloom_target_image,
                &engine->bloom_pingpong_image
            };
            transition_gpu_images(cmd, 2, bloom_prepare_horizontal_blur_barriers, bloom_prepare_horizontal_blur_barrier_images);

            // Horizontal blur pass
            {
                // Bind descriptor set for Scene
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_gaussian_blur_horizontal.layout,
                    SCENE_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->scene_descriptor_set,
                    0,
                    NULL
                );

                // Bind bloom target to sample during postprocess
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_gaussian_blur_horizontal.layout,
                    POSTPROCESS_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->bloom_target_postprocess_descriptor_set,
                    0,
                    NULL
                );
                // Color attachment
                VkRenderingAttachmentInfo color_attachment = {
                    .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext               = NULL,
                    .imageView           = engine->bloom_pingpong_image.image_view,
                    .imageLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // Note this must match the image barrier above
                    .resolveMode         = VK_RESOLVE_MODE_NONE,
                    .resolveImageView    = VK_NULL_HANDLE,
                    .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp              = VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // We are overwriting it fully
                    .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue          = {}
                };

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = 1,
                    .color_attachments       = &color_attachment,
                    .color_attachment_images = &engine->bloom_pingpong_image.image,
                    .depth_attachment        = NULL,
                    .depth_attachment_image  = NULL,

                    .render_area             = bloom_render_area,
                    .viewport                = bloom_viewport,
                    .scissor                 = bloom_scissor
                };

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = rendering_attachments.render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = rendering_attachments.color_attachment_count,
                    .pColorAttachments     = rendering_attachments.color_attachments,
                    .pDepthAttachment      = rendering_attachments.depth_attachment,
                    .pStencilAttachment    = NULL  // TODO: Include stencil attachment
                };

                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->gp_bloom_gaussian_blur_horizontal.pipeline);

                    // Dynamic pipeline states: Set viewport and scissor.
                    vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
                    vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }

            // Transition: now we sample the bloom pingpong target and write to the bloom target
            VkImageMemoryBarrier2 bloom_prepare_vertical_blur_barriers[2] = {
                vklayer_specify_image_transition_barrier(
                    engine->bloom_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    engine->bloom_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->bloom_pingpong_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    engine->bloom_pingpong_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* bloom_prepare_vertical_blur_barrier_images[2] = {
                &engine->bloom_target_image,
                &engine->bloom_pingpong_image
            };
            transition_gpu_images(cmd, 2, bloom_prepare_vertical_blur_barriers, bloom_prepare_vertical_blur_barrier_images);

            // Vertical blur pass
            {
                // Bind descriptor set for Scene
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_gaussian_blur_vertical.layout,
                    SCENE_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->scene_descriptor_set,
                    0,
                    NULL
                );

                // Bind bloom target to sample during postprocess
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_gaussian_blur_vertical.layout,
                    POSTPROCESS_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->bloom_pingpong_postprocess_descriptor_set,
                    0,
                    NULL
                );
                // Color attachment
                VkRenderingAttachmentInfo color_attachment = {
                    .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext               = NULL,
                    .imageView           = engine->bloom_target_image.image_view,
                    .imageLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // Note this must match the image barrier above
                    .resolveMode         = VK_RESOLVE_MODE_NONE,
                    .resolveImageView    = VK_NULL_HANDLE,
                    .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp              = VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // We are overwriting it fully
                    .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue          = {}
                };

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = 1,
                    .color_attachments       = &color_attachment,
                    .color_attachment_images = &engine->bloom_target_image.image,
                    .depth_attachment        = NULL,
                    .depth_attachment_image  = NULL,

                    .render_area             = bloom_render_area,
                    .viewport                = bloom_viewport,
                    .scissor                 = bloom_scissor
                };

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = rendering_attachments.render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = rendering_attachments.color_attachment_count,
                    .pColorAttachments     = rendering_attachments.color_attachments,
                    .pDepthAttachment      = rendering_attachments.depth_attachment,
                    .pStencilAttachment    = NULL  // TODO: Include stencil attachment
                };

                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->gp_bloom_gaussian_blur_vertical.pipeline);

                    // Dynamic pipeline states: Set viewport and scissor.
                    vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
                    vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }

            
            // Now the blurred brightness is stored in the bloom_target_image
            // For the bloom effect, we combine this to what's in the rendertarget
            // So...
            // - Read from bloom_target_image and render_target_image (so transition those to SHADER_READ_ONLY_OPTIMAL).
            // - Write to the render_target_pingpong_image (so transition that to COLOR_ATTACHMENT_OPTIMAL)
            
            VkImageMemoryBarrier2 bloom_apply_to_pingpong_rendertarget_barriers[3] = {
                vklayer_specify_image_transition_barrier(
                    engine->bloom_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    engine->bloom_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->render_target_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    engine->render_target_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->render_target_pingpong_image.image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    engine->render_target_pingpong_image.current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                )
            };
            GPU_Image* bloom_apply_to_pingpong_rendertarget_barrier_images[3] = {
                &engine->bloom_target_image,
                &engine->render_target_image,
                &engine->render_target_pingpong_image
            };
            transition_gpu_images(cmd, 3, bloom_apply_to_pingpong_rendertarget_barriers, bloom_apply_to_pingpong_rendertarget_barrier_images);


            // Apply bloom with the gp_bloom_apply pipeline
            {
                // Bind descriptor set for Scene
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_apply.layout,
                    SCENE_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->scene_descriptor_set,
                    0,
                    NULL
                );

                // Bind scene_texture and bloom_texture
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    engine->gp_bloom_apply.layout,
                    BLOOM_APPLY_DESCRIPTOR_SET_INDEX,
                    1,
                    &engine->bloom_apply_descriptor_set,
                    0,
                    NULL
                );

                // Color attachment is the render target pingpong image
                VkRenderingAttachmentInfo color_attachment = {
                    .sType               = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext               = NULL,
                    .imageView           = engine->render_target_pingpong_image.image_view,
                    .imageLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // Note this must match the image barrier above
                    .resolveMode         = VK_RESOLVE_MODE_NONE,
                    .resolveImageView    = VK_NULL_HANDLE,
                    .resolveImageLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp              = VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // We are overwriting it fully
                    .storeOp             = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue          = {}
                };

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = 1,
                    .color_attachments       = &color_attachment,
                    .color_attachment_images = &engine->render_target_pingpong_image.image,
                    .depth_attachment        = NULL,
                    .depth_attachment_image  = NULL,

                    .render_area             = render_area,
                    .viewport                = viewport,
                    .scissor                 = scissor
                };

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = rendering_attachments.render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = rendering_attachments.color_attachment_count,
                    .pColorAttachments     = rendering_attachments.color_attachments,
                    .pDepthAttachment      = rendering_attachments.depth_attachment,
                    .pStencilAttachment    = NULL  // TODO: Include stencil attachment
                };

                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->gp_bloom_apply.pipeline);

                    // Dynamic pipeline states: Set viewport and scissor.
                    vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
                    vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }

        }

        // NOTE: The final image may be in either the render target or the pingpong buffer
        // Depending on even or odd number of postprocessing steps. For now it's simply:
        // (God this needs a high level render graph system, i want to be able to draw all this shit like a graph and code gen it)
        GPU_Image* current_render_target = &engine->render_target_image;
        VkDescriptorSet* current_postprocess_descriptor_set = &engine->postprocess_descriptor_set;
        if (engine->do_bloom)
        {
            current_render_target = &engine->render_target_pingpong_image;
            current_postprocess_descriptor_set = &engine->postprocess_pingpong_descriptor_set;
        }
        
        // Put swapchain image into initial format
        // NOTE: Currently updating the tracking of it's state manually. (ugh there is a lot of shit in vulkan AAAAAAAAAAAAAA)
        // VkPipelineStageFlags2 swapchain_image_before_stage        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        // VkAccessFlags2        swapchain_image_before_access_mask  = VK_ACCESS_2_NONE;
        // VkImageLayout         swapchain_image_before_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout swapchain_image_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        u32 swapchain_current_queue_family = engine->queue_family_indices.present_family;
        {
            VkImageLayout new_swapchain_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            u32 new_swapchain_queue_family = engine->queue_family_indices.graphics_family;

            VkImageMemoryBarrier2 swapchain_initial_barrier = vklayer_specify_image_transition_barrier(
                engine->swapchain_images[swapchain_image_index], vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                /* Before */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                swapchain_image_current_layout,
                swapchain_current_queue_family,
                /* After */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                new_swapchain_image_layout,
                new_swapchain_queue_family  // Move to graphics family
            );
            vklayer_cmd_transition_images(cmd, 1, &swapchain_initial_barrier);

            swapchain_image_current_layout = new_swapchain_image_layout;
            swapchain_current_queue_family = new_swapchain_queue_family;
        }


        // FUN FUTURE TEST?:
        // Hey, what if... i apply bloom after the mosaic filter

        if (engine->do_postprocessing)
        {
            //
            // POSTPROCESSING
            //

            // We will sample the render target and output color to the swapchain image
            VkImageLayout new_swapchain_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            u32 new_swapchain_queue_family           = engine->queue_family_indices.graphics_family;

            VkImageMemoryBarrier2 render_target_to_swapchain_barriers[2] = {
                vklayer_specify_image_transition_barrier(
                    current_render_target->image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    current_render_target->current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->swapchain_images[swapchain_image_index], vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    swapchain_image_current_layout,
                    engine->queue_family_indices.present_family,
                    /* After */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    new_swapchain_image_layout,
                    new_swapchain_queue_family  // Move to graphics family
                )
            };
            GPU_Image* render_target_to_swapchain_barrier_images[2] = {
                current_render_target,
                NULL
            };
            transition_gpu_images(cmd, 2, render_target_to_swapchain_barriers, render_target_to_swapchain_barrier_images);

            swapchain_image_current_layout = new_swapchain_image_layout;
            swapchain_current_queue_family = new_swapchain_queue_family;
            
            // Bind descriptor set for Scene
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                engine->gp_fullscreen_tri_mosaic.layout,
                SCENE_DESCRIPTOR_SET_INDEX,
                1,
                &engine->scene_descriptor_set,
                0,
                NULL
            );

            // Bind image to postprocess
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                engine->gp_fullscreen_tri_mosaic.layout,
                POSTPROCESS_DESCRIPTOR_SET_INDEX,
                1,
                current_postprocess_descriptor_set,
                0,
                NULL
            );

            {
                // Color attachment
                VkImage color_images[1] = { engine->swapchain_images[swapchain_image_index] };
                VkRenderingAttachmentInfo color_attachments[1] = {};
                color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                color_attachments[0].imageView   = engine->swapchain_image_views[swapchain_image_index];
                color_attachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // Note this must match the image barrier above
                color_attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // We are overwriting it anyway
                color_attachments[0].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

                // Specify attachments for this graphics pipeline's renderpass
                RenderingAttachments rendering_attachments = {
                    .color_attachment_count  = sizeof(color_attachments) / sizeof(VkRenderingAttachmentInfo),
                    .color_attachments       = color_attachments,
                    .color_attachment_images = color_images,
                    .depth_attachment        = NULL,
                    .depth_attachment_image  = NULL,

                    .render_area             = render_area,
                    .viewport                = viewport,
                    .scissor                 = scissor
                };

                // Pass attachment information to render_info so we can begin rendering    
                VkRenderingInfo render_info = {
                    .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                 = NULL,
                    .flags                 = 0,
                    .renderArea            = rendering_attachments.render_area,
                    .layerCount            = 1,
                    .viewMask              = 0,
                    .colorAttachmentCount  = rendering_attachments.color_attachment_count,
                    .pColorAttachments     = rendering_attachments.color_attachments,
                    .pDepthAttachment      = rendering_attachments.depth_attachment,
                    .pStencilAttachment    = NULL  // TODO: Include stencil attachment
                };

                // Draw single triangle
                vkCmdBeginRendering(cmd, &render_info);
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->gp_fullscreen_tri_mosaic.pipeline);

                    // Dynamic pipeline states: Set viewport and scissor.
                    vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
                    vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }
                vkCmdEndRendering(cmd);
            }


            // Turn swapchain image into a presentable format
            VkImageMemoryBarrier2 presentation_image_barrier = vklayer_specify_image_transition_barrier(
                engine->swapchain_images[swapchain_image_index], vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                /* Before */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                engine->queue_family_indices.graphics_family,
                /* After */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                engine->queue_family_indices.present_family  // Move back to present family
            );
            vklayer_cmd_transition_images(cmd, 1, &presentation_image_barrier);

        }
        else
        {
            //
            //  --- BLIT RENDER TARGET TO SWAPCHAIN IMAGE AND PRESENT ---
            //

            // Next we need to blit our render result to the current swapchain image (aka what ends up on the screen)
            //
            // Transition the render target and the swapchain image into transfer layouts (We will transfer from render target to swapchain image)
            VkImageLayout new_swapchain_image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            u32 new_swapchain_queue_family           = engine->queue_family_indices.graphics_family;

            VkImageMemoryBarrier2 render_target_to_swapchain_barriers[2] = {
                vklayer_specify_image_transition_barrier(
                    current_render_target->image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    current_render_target->current_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    /* After */
                    VK_PIPELINE_STAGE_2_BLIT_BIT,  // Basically the same as transfer in terms of memory synch but allows fine grained scheduling of blit commands (which some drivers may implement with a shader since filtering can be involved).
                    VK_ACCESS_2_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED
                ),
                vklayer_specify_image_transition_barrier(
                    engine->swapchain_images[swapchain_image_index], vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                    /* Before */
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    swapchain_image_current_layout,
                    swapchain_current_queue_family,
                    /* After */
                    VK_PIPELINE_STAGE_2_BLIT_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    new_swapchain_image_layout,
                    new_swapchain_queue_family  // Move to graphics family
                )
            };
            GPU_Image* render_target_to_swapchain_barrier_images[2] = {
                current_render_target,
                NULL
            };
            transition_gpu_images(cmd, 2, render_target_to_swapchain_barriers, render_target_to_swapchain_barrier_images);

            swapchain_image_current_layout = new_swapchain_image_layout;
            swapchain_current_queue_family = new_swapchain_queue_family;


            // Blit render target to current swapchain image
            vklayer_cmd_blit_image_to_image(cmd, current_render_target->image, engine->swapchain_images[swapchain_image_index], engine->render_target_extent, engine->swapchain_extent);

            // Turn swapchain image into a presentable format
            VkImageMemoryBarrier2 presentation_image_barrier = vklayer_specify_image_transition_barrier(
                engine->swapchain_images[swapchain_image_index], vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
                /* Before */
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                engine->queue_family_indices.graphics_family,
                /* After */
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                engine->queue_family_indices.present_family  // Move back to present family
            );
            vklayer_cmd_transition_images(cmd, 1, &presentation_image_barrier);
        }
    }
    VK_CHECK(vkEndCommandBuffer(cmd));  // Finalise command buffer: It can now only be executed, no more commands may be recorded.

    //
    // END OF COMMAND BUFFER RECORDING
    //

    

    // Prepare submission of cmd to the queue.
    // - Requires waiting on the present semaphore (signalled when the swapchain is ready)
    // - We will signal the render semaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmd_info = vklayer_command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo wait_info = vklayer_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        engine->swapchain_image_acquired_semaphores[image_acquired_semaphore_id]
    );
    VkSemaphoreSubmitInfo signal_info = vklayer_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        engine->swapchain_image_render_semaphores[swapchain_image_index]
    );

    // Submit command buffer to the queue and execute it.
    VkSubmitInfo2 submit = vklayer_submit_info(&cmd_info, &signal_info, &wait_info);
    VK_CHECK(vkQueueSubmit2(engine->graphics_queue, 1, &submit, current_frame->render_fence));


    // Present to screen
    // (after waiting on the render semaphore so that all drawing is completed first)
    VkPresentInfoKHR present_info = {
        .sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext               = NULL,
        .waitSemaphoreCount  = 1,
        .pWaitSemaphores     = &engine->swapchain_image_render_semaphores[swapchain_image_index],
        .swapchainCount      = 1,
        .pSwapchains         = &engine->swapchain,
        .pImageIndices       = &swapchain_image_index,
        .pResults            = NULL  // <- Only relevant when swapchainCount > 1
    };
    VkResult present_result = vkQueuePresentKHR(engine->presentation_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
    {
        engine->window_resizing = 1;
    }
    else
    {
        VK_CHECK(present_result);
    }
}

void
vk_run(VulkanEngine* engine)
{
    double last_time;
    double time = glfwGetTime();
    while (!glfwWindowShouldClose(engine->window))
    {
        // Calculate delta time
        last_time = time;
        time = glfwGetTime();
        float dt = (float)(time - last_time);

        // Keep last frame's input to determine press/release of buttons
        engine->last_input = engine->input;

        // Also keep last render mode so we know if we need to recreate graphics pipelines
        engine->last_render_mode = engine->render_mode;
    
        // Process window/input events
        glfwPollEvents();
        if (engine->window_minimized)
        {
            glfwWaitEventsTimeout(0.1);
            continue;
        }
        if (engine->window_resizing)
        {
            engine->window_resizing = 0;

            // vkQueueWaitIdle(engine->presentation_queue);
            vkDeviceWaitIdle(engine->device);
            create_or_recreate_swapchain(engine);  // NOTE: This almost always doesn't recreate graphics pipelines unless image format changes
            continue;
        }


        // On rendermode change, recreate the pipelines and they'll use different shaders
        if (engine->render_mode != engine->last_render_mode)
        {
            vkDeviceWaitIdle(engine->device);
            destroy_all_graphics_pipelines(engine);
            create_all_graphics_pipelines(engine);
        }

        vk_tick_game(engine, dt);
        vk_draw(engine);
        ++engine->frame_number;


        // Display fps in window title for now
        static int num_frames_this_second = 0;
        static double last_reset = 0.0;
        if (time - last_reset > 1.0)
        {
            char window_title[256] = {};
            snprintf(window_title, 255, "%s | %s, fps: %d\n", app_name, get_render_mode_name(engine->render_mode), num_frames_this_second);
            glfwSetWindowTitle(engine->window, window_title);

            last_reset = time;
            num_frames_this_second = 0;
        }
        else
        {
            ++num_frames_this_second;
        }
    }
}



// GLFW Callbacks
//



void
glfw_window_size_callback(GLFWwindow* window, int width, int height)
{
    VulkanEngine* engine = (VulkanEngine*)glfwGetWindowUserPointer(window);
    assert(engine);

    if (width == 0 || height == 0)
    {
        VERBOSE_LOG("Window minimized: %f\n", glfwGetTime());
        engine->window_minimized = 1;
        return;
    }
    else if (engine->window_minimized)
    {
        VERBOSE_LOG("Window unminimized: %f\n", glfwGetTime());
        engine->window_minimized = 0;
    }

    engine->window_extents.width = width;
    engine->window_extents.height = height;
    engine->window_resizing = 1;
}

void
glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    VulkanEngine* engine = (VulkanEngine*)glfwGetWindowUserPointer(window);
    assert(engine);

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, 1);
    }

    int button_action = -1;
    if (action == GLFW_PRESS)        button_action = 1;
    else if (action == GLFW_RELEASE) button_action = 0;

    if (button_action != -1)  // Only listen to GLFW_PRESS and GLFW_RELEASE keyboard events
    {
        switch (key)
        {
            // Camera movement:
            // WASD lateral movement, E-up, Q-down.
            case (GLFW_KEY_W): engine->input.key_forward  = button_action; break;
            case (GLFW_KEY_S): engine->input.key_back     = button_action; break;
            case (GLFW_KEY_A): engine->input.key_left     = button_action; break;
            case (GLFW_KEY_D): engine->input.key_right    = button_action; break;
            case (GLFW_KEY_E): engine->input.key_up       = button_action; break;
            case (GLFW_KEY_Q): engine->input.key_down     = button_action; break;
            // Change speed with LShift (faster) and Ctrl (slower)
            case (GLFW_KEY_LEFT_SHIFT):   engine->input.key_move_faster = button_action; break;
            case (GLFW_KEY_LEFT_CONTROL): engine->input.key_move_slower = button_action; break;
        }
    }

    // Toggle things on button release:
    if (action == GLFW_RELEASE)
    {
        switch (key)
        {
            case (GLFW_KEY_P):
                engine->do_postprocessing = !engine->do_postprocessing;
                break;
            
            case (GLFW_KEY_B):
                engine->do_bloom = !engine->do_bloom;
                break;

            // Switch render mode with keys 1-4
            case (GLFW_KEY_1):
                // Repeating KEY_1 toggles between deferred and forward.
                if (engine->render_mode == RENDERMODE_DEFAULT_DEFERRED)
                {
                    engine->render_mode = RENDERMODE_DEFAULT_FORWARD;
                }
                else
                {
                    engine->render_mode = RENDERMODE_DEFAULT_DEFERRED;
                }
                break;
            case (GLFW_KEY_2): engine->render_mode = RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS; break;
            case (GLFW_KEY_3): engine->render_mode = RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH; break;
            case (GLFW_KEY_4): engine->render_mode = RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES; break;
            case (GLFW_KEY_5): engine->render_mode = RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW; break;
            case (GLFW_KEY_6): engine->render_mode = RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING; break;
            case (GLFW_KEY_7): engine->render_mode = RENDERMODE_DEBUGVIZ_MESH_DENSITY; break;

            case (GLFW_KEY_ENTER):
                float red   = fmodf(engine->camera_position.x, 1.0f);
                float green = fmodf(engine->camera_position.z, 1.0f);
                float blue  = 1.0f - red;
                PointLight new_light = make_point_light(
                    engine->camera_position,
                    glm::vec4(red, green, blue, 75.0f)
                );
                scene_add_pointlight(engine, new_light);

                VERBOSE_LOG("Added point light%d:  pos (%ff, %ff, %ff), radius %f\n", engine->scene.point_lights_size,
                    new_light.pos_and_radius.x, new_light.pos_and_radius.y, new_light.pos_and_radius.z, new_light.pos_and_radius.w
                );

                glm::vec3 cam_dir = get_camera_direction_from_view(get_camera_view_matrix(engine->camera_position, engine->camera_pitch, engine->camera_yaw));
                VERBOSE_LOG("Current camera direction (%ff, %ff, %ff)\n", cam_dir.x, cam_dir.y, cam_dir.z);

                break;
        }
    }

    // TODO: I would like to set the glfw focus lossed callback so that I can disable
    // the button down events, because if I get a notification it doesn't release the
    // keys and keeps moving which is annoying (annoying in lots of released games too).
}


void
glfw_mouse_motion_callback(GLFWwindow* window, double xpos, double ypos)
{
    VulkanEngine* engine = (VulkanEngine*)glfwGetWindowUserPointer(window);

    // Update mouse position
    engine->input.mouse_xpos = xpos;
    engine->input.mouse_ypos = ypos;

}

void
glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    VulkanEngine* engine = (VulkanEngine*)glfwGetWindowUserPointer(window);

    // When the right key is pressed, we toggle mouse capture for camera movement
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        engine->input.mouse_button_right = action == GLFW_PRESS;

        // Toggle mouse capture for camera:
        if (engine->input.mouse_button_right && !engine->last_input.mouse_button_right)
        {
            engine->input.mouse_is_captured = !engine->input.mouse_is_captured;

            if (engine->input.mouse_is_captured)
            {
                glfwSetInputMode(engine->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else
            {
                glfwSetInputMode(engine->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        engine->input.mouse_button_left = action == GLFW_PRESS;
    }

}

// Engine Struct Functions
//

const char*
get_render_mode_name(RenderMode render_mode)
{
    switch (render_mode)
    {
        #define AS_STRING(name, desc) case name: return #name;
        RENDERMODE_LIST(AS_STRING)
        #undef AS_STRING
        default: return "UNKNOWN_RENDERMODE";
    }
}



// NOTE: Need to free SwapChainSupportDetails.formats and .present_modes after use
SwapChainSupportDetails
get_and_alloc_swap_chain_support_details(VulkanEngine* engine, VkPhysicalDevice physical_device)
{
    SwapChainSupportDetails details = {};

    // Capabilities are based on the VkPhysicalDevice and the VkSurfaceKHR
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, engine->surface, &details.capabilities);

    // Query the supported surface formats 
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, engine->surface, &details.format_count, NULL);
    if (details.format_count != 0)
    {
        details.formats = (VkSurfaceFormatKHR*)L_calloc(details.format_count, sizeof(VkSurfaceFormatKHR), &engine->main.tt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, engine->surface, &details.format_count, details.formats);
    }

    // Query the presentation modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, engine->surface, &details.present_mode_count, NULL);
    if (details.present_mode_count != 0)
    {
        details.present_modes = (VkPresentModeKHR*)L_calloc(details.present_mode_count, sizeof(VkPresentModeKHR), &engine->main.tt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, engine->surface, &details.present_mode_count, details.present_modes);
    }

    return details;
}

void
free_swap_chain_support_details(SwapChainSupportDetails details, ThreadAllocTracker* alloc_tracker)
{
    if (details.formats)       L_free(details.formats, alloc_tracker);
    if (details.present_modes) L_free(details.present_modes, alloc_tracker);
}

QueueFamilyIndices
get_physical_device_queue_family_indices(VulkanEngine* engine, VkPhysicalDevice physical_device)
{
    QueueFamilyIndices queue_family_indices = {};

    // Each queue family index initialized to -1 so we can test for failure
    memset(&queue_family_indices, -1, sizeof(QueueFamilyIndices));

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)L_calloc(queue_family_count, sizeof(VkQueueFamilyProperties), &engine->main.tt);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    // Check for support of each required queue for each queue family
    b32* queue_families_support_for_graphics = (b32*)L_calloc(queue_family_count, sizeof(b32), &engine->main.tt);
    b32* queue_families_support_for_presentation = (b32*)L_calloc(queue_family_count, sizeof(b32), &engine->main.tt);
    // etc.
    for (u32 i = 0; i < queue_family_count; ++i)
    {
        VERBOSE_LOG("--- Queue family %d's bits: ", i);
        vklayer_print_queueflagbits((VkQueueFlagBits)queue_families[i].queueFlags);

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queue_families_support_for_graphics[i] = 1;
        }

        VkBool32 presentation_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, engine->surface, &presentation_support);
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
    L_free(queue_families_support_for_graphics, &engine->main.tt);
    L_free(queue_families_support_for_presentation, &engine->main.tt);
    L_free(queue_families, &engine->main.tt);

    return queue_family_indices;
}

// NOTE: Negative score means unsuitable device
int
score_physical_device_and_check_required_features(VulkanEngine* engine, VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    u32 supported_version_major = VK_API_VERSION_MAJOR(device_properties.apiVersion);
    u32 supported_version_minor = VK_API_VERSION_MINOR(device_properties.apiVersion);
    u32 supported_version_patch = VK_API_VERSION_PATCH(device_properties.apiVersion);

    VERBOSE_LOG("--- name: %s\n", device_properties.deviceName);
    VERBOSE_LOG("--- supports up to Vulkan %d.%d.%d\n",
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
                VERBOSE_LOG("--- is dGPU... lovely :)\n");
                suitability_score += 1000;    
                break;

            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                VERBOSE_LOG("--- is iGPU... decent :^)\n");
                suitability_score += 500;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                // TODO: vGPU could be better or worse than the iGPU on a machine, need to check more features in the future (good enough for now)
                VERBOSE_LOG("--- is vGPU (virtual GPU)... who knows? TODO: Further checks for if it's a good vGPU e.g. cloud dGPU\n");
                suitability_score += 200;
                break;

            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                VERBOSE_LOG("--- is CPU... meh :(\n");
                suitability_score += 0;
                break;
            
            default:
                VERBOSE_LOG("--- dunno what type of device this is :(\n");
                suitability_score += 0;
        }
        
        // Check memory properties of this physical device
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        // Display memory heaps and types
        VERBOSE_LOG("--- Heaps: has %d memory heap.\n", memory_properties.memoryHeapCount);
        for (u32 i = 0; i < memory_properties.memoryHeapCount; ++i)
        {
            VERBOSE_LOG("--- - heap %i: %lu MiB, flags: ", i, memory_properties.memoryHeaps[i].size / (1024*1024));
            vklayer_print_memoryheapflagbits(memory_properties.memoryHeaps[i].flags);
        }

        VERBOSE_LOG("--- Memory types: %d types\n", memory_properties.memoryTypeCount);
        for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
        {
            VERBOSE_LOG("--- - type %d: from heap %d, flags: ", i, memory_properties.memoryTypes[i].heapIndex);
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
            VERBOSE_LOG("--- Anisotropic filtering: %d\n", device_features.features.samplerAnisotropy);
            all_required_features_supported = all_required_features_supported  && device_features.features.samplerAnisotropy;
            if (device_features.features.samplerAnisotropy)
            {
                printf("--- - Max anisotropy: %f\n", device_properties.limits.maxSamplerAnisotropy);
            }
            
            VERBOSE_LOG("--- Synchronization 2: %d\n", vk13_features.synchronization2);
            all_required_features_supported = all_required_features_supported  && vk13_features.synchronization2;
            VERBOSE_LOG("--- Dynamic Rendering: %d\n", vk13_features.dynamicRendering);
            all_required_features_supported = all_required_features_supported  && vk13_features.dynamicRendering;
            
            // Maintenance5 means we don't need VkShaderModule.
            VERBOSE_LOG("--- Maintenance 5: %d\n", vk14_features.maintenance5);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance5;
            VERBOSE_LOG("--- Maintenance 6: %d\n", vk14_features.maintenance6);
            all_required_features_supported = all_required_features_supported  && vk14_features.maintenance6;

            // Buffer device address for GPU pointers
            VERBOSE_LOG("--- Buffer Device Address: %d (not using it at the moment though)\n", buffer_device_address_features.bufferDeviceAddress);
            all_required_features_supported = all_required_features_supported  && buffer_device_address_features.bufferDeviceAddress;

            // Geometry shaders
            VERBOSE_LOG("--- Geometry shaders: %d\n", device_features.features.geometryShader);
            all_required_features_supported = all_required_features_supported && device_features.features.geometryShader;
        }

        // Requires the necessary queue families
        QueueFamilyIndices indices = get_physical_device_queue_family_indices(engine, physical_device);
        b32 all_required_queues_supported = 
                indices.graphics_family != -1 &&
                indices.present_family != -1;

        // Requires some device extensions
        b32 all_required_extensions_available = 0;
        if (all_required_queues_supported)
        {
            u32 available_extensions_count;
            vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_count, NULL);
            VkExtensionProperties* available_extensions = (VkExtensionProperties*)L_calloc(available_extensions_count, sizeof(VkExtensionProperties), &engine->main.tt);
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

            L_free(available_extensions, &engine->main.tt);
        }

        // Swapchain support details
        b32 is_swapchain_adequate = 0;
        if (all_required_extensions_available)  // Must only query swapchain support after making sure the previous extensions to do so were available
        {
            SwapChainSupportDetails details = get_and_alloc_swap_chain_support_details(engine, physical_device);

            is_swapchain_adequate = details.format_count > 0 && details.present_mode_count > 0;
            
            free_swap_chain_support_details(details, &engine->main.tt);
        }

        VERBOSE_LOG("--- Checking against requirements: VersionUpToDate:%d, Features:%d, Queues:%d, Extensions:%d, SwapChain:%d\n",
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

void
create_or_recreate_swapchain(VulkanEngine* engine)
{
    // NOTE: If a swapchain already exists, Vulkan wants the handle to the old swapchain
    // passed to the swapchain create info of the new one.
    // Hence create_or_recreate instead of just destroy() then create() when one already exists.

    // Get support details for swap chain
    SwapChainSupportDetails details = get_and_alloc_swap_chain_support_details(engine, engine->physical_device);

    VkSurfaceFormatKHR chosen_format;
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

    VkPresentModeKHR chosen_present_mode;
    chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Only FIFO is guarunteed to be available
    for (u32 i = 0; i < details.present_mode_count; ++i)
    {
        if (engine->uncapped_fps)
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

    VkExtent2D chosen_swap_extent;
    if (details.capabilities.currentExtent.width != UINT32_MAX)
    {
        chosen_swap_extent = details.capabilities.currentExtent;
    }
    else
    {
        // Requesting in pixel coordinates not screen coordinates because some HiDPI displays make a distinction there
        // and we want to actually render to each and every pixel available on the monitor
        int width, height;
        glfwGetFramebufferSize(engine->window, &width, &height);

        VkExtent2D actual_extent = { (u32)width, (u32)height };

        // Must be clamped between the min and max extents allowed by the implementation
        actual_extent.width = std::clamp(actual_extent.width,
            details.capabilities.minImageExtent.width,
            details.capabilities.maxImageExtent.width
        );
        actual_extent.height = std::clamp(actual_extent.height,
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

    // Finally create the swap chain
    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = engine->surface;
    swapchain_create_info.minImageCount = min_image_count;
    swapchain_create_info.imageFormat = chosen_format.format;
    swapchain_create_info.imageColorSpace = chosen_format.colorSpace;
    swapchain_create_info.imageExtent = chosen_swap_extent;
    swapchain_create_info.imageArrayLayers = 1;  // One layer since we aren't making a stereoscopic 3D application
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // OLD: (I do explicit ownership transfer with VK_SHARING_MODE_EXCLUSIVE now)
    // if (engine->queue_family_indices.graphics_family != engine->queue_family_indices.present_family)
    // {
    //     // TODO: when gfx_queue!=present_queue: VK_SHARING_MODE_EXCLUSIVE has better performance
    //     // but first we need to seperate it so only the presentation queue uses the swapchain images (i think)
    //     // or just transfer ownership and use barriers
    //     swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    //     swapchain_create_info.queueFamilyIndexCount = 2;
    //     swapchain_create_info.pQueueFamilyIndices = (u32*)engine->queue_family_indices.array;
    // }
    // else
    // {
    //     swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    //     swapchain_create_info.queueFamilyIndexCount = 0;
    //     swapchain_create_info.pQueueFamilyIndices = NULL;
    // }

    // NOTE: The correct ownership transfers of swapchain images in vk_draw now get performed
    // so it is safe to use VK_SHARING_MODE_EXCLUSIVE even if present queue != graphics queue.
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = NULL;

    swapchain_create_info.preTransform = details.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // Alpha is opaque, i.e. no blending with other windows
    swapchain_create_info.presentMode = chosen_present_mode;
    swapchain_create_info.clipped = VK_TRUE;  // When another window is partially in the way, we don't care what the covered pixels colours are (unless you are reading from them later for some special reason)

    VkSwapchainKHR old_swapchain = engine->swapchain;
    if (engine->is_a_swapchain_created)
    {
        // Passing the old swapchain can allow the driver to reuse some resources
        swapchain_create_info.oldSwapchain = old_swapchain;
    }
    else
    {
        swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;
    }

    
    // Create the swapchain
    VK_CHECK(vkCreateSwapchainKHR(engine->device, &swapchain_create_info, NULL, &engine->swapchain));
    free_swap_chain_support_details(details, &engine->main.tt);

    if (engine->is_a_swapchain_created)
    {
        // Vulkan never implicitly destroys the old swapchain we passed to the new create info, we still must destroy it.
        destroy_swapchain(engine, old_swapchain);
    }


    // Save the chosen format and extent so we can copy/transfer correctly to it later
    VkFormat old_format = engine->swapchain_image_format;
    engine->swapchain_image_format = chosen_format.format;
    engine->swapchain_extent = chosen_swap_extent;

    // Retrieve the handles of the images created by the swapchaip
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &engine->swapchain_image_count, NULL);
    engine->swapchain_images = (VkImage*)L_calloc(engine->swapchain_image_count, sizeof(VkImage), &engine->main.tt);
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &engine->swapchain_image_count, engine->swapchain_images);

    VERBOSE_LOG("Swapchain created.\n");
    VERBOSE_LOG("- logical resolution(%d, %d)\n", engine->swapchain_extent.width, engine->swapchain_extent.height);
    
    
    // Create Image Views for SwapChain
    engine->swapchain_image_views = (VkImageView*)L_calloc(engine->swapchain_image_count, sizeof(VkImageView), &engine->main.tt);
    for (u32 i = 0; i < engine->swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = engine->swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = engine->swapchain_image_format;
        
        // Use no swizzle for the color components
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // Specify as color target with no mipmapping
        image_view_create_info.subresourceRange.aspectMask= VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(engine->device, &image_view_create_info, NULL, &engine->swapchain_image_views[i]));
    }


    // Create Semaphores for SwapChain
    engine->swapchain_image_acquired_semaphores = (VkSemaphore*)L_calloc(engine->swapchain_image_count, sizeof(VkSemaphore), &engine->main.tt);
    engine->swapchain_image_render_semaphores = (VkSemaphore*)L_calloc(engine->swapchain_image_count, sizeof(VkSemaphore), &engine->main.tt);

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = NULL;
    semaphore_create_info.flags = 0;
    for (u32 i = 0; i < engine->swapchain_image_count; ++i)
    {
        engine->swapchain_image_acquired_semaphores[i] = VK_NULL_HANDLE;
        engine->swapchain_image_render_semaphores[i] = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphore_create_info, NULL, &engine->swapchain_image_acquired_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphore_create_info, NULL, &engine->swapchain_image_render_semaphores[i]));
    }
    

    // Swapchain creation is completed
    VERBOSE_LOG("- created %d swapchain image views.\n", engine->swapchain_image_count);
    engine->is_a_swapchain_created = 1;
    

    // Create render target
    VkExtent3D render_image_extent = {
        engine->window_extents.width,
        engine->window_extents.height,
        1
    };

    // Color buffer (final image) (Also an equivalent pingpong buffer for chaining postprocessing steps later)
    const VkFormat render_target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkImageUsageFlags render_target_usage = 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |                                // <- Colour attachment for fragment shader
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |  // <- Need to be able to copy rendered image to swapchain image
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;             // <- For compute and sampling as a texture during postprocessing
    engine->render_target_image          = create_render_target_attachment(engine, 0, render_target_format, render_image_extent, render_target_usage);
    engine->render_target_pingpong_image = create_render_target_attachment(engine, 0, render_target_format, render_image_extent, render_target_usage);
    engine->render_target_extent.width = engine->render_target_image.image_extent.width;
    engine->render_target_extent.height = engine->render_target_image.image_extent.height;

    // Depth
    engine->render_target_depth = create_render_target_attachment(engine, 1,
        VK_FORMAT_UNDEFINED,  // TODO: Depth format currently determine inside create_render_target_attachment() but maybe I should do it here instead
        render_image_extent,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT  // <- Depth-buffer used as G-Buffer in lighting pass.
    );

    // DEFERRED RENDERING:
    // G-Buffers for deferred rendering must be usable as both an attachment and a texture to be sampled from.
  
    engine->render_target_deferred_albedo_roughness = create_render_target_attachment(engine, 0,
        VK_FORMAT_R8G8B8A8_SRGB, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
    
    // NOTE: 10-bit normals caused lots of colour banding so only use the 16-bit format.
    engine->render_target_deferred_normal_metalness = create_render_target_attachment(engine, 0,
        VK_FORMAT_R16G16B16A16_SNORM, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
    // engine->render_target_deferred_normal_metalness = create_render_target_attachment(engine, 0, VK_FORMAT_A2B10G10R10_UNORM_PACK32, render_image_extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);  // OLD.

    engine->render_target_deferred_emissive_ao = create_render_target_attachment(engine, 0,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        render_image_extent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );


    // SHADOW MAPS:
    const u32 shadow_map_resolution = 1024;
    engine->shadow_map_depth = create_render_target_attachment(engine, 1,
        VK_FORMAT_UNDEFINED,  // Currently this means it uses D32_SFLOAT
        (VkExtent3D){ shadow_map_resolution, shadow_map_resolution, 1 },
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );


    // Bloom target (lower res of render_target_image for extracting the bright parts)
    VkExtent3D bloom_target_extents = { render_image_extent.width / 2, render_image_extent.height / 2, 1 };
    engine->bloom_target_extent = { bloom_target_extents.width, bloom_target_extents.height };
    const VkFormat bloom_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkImageUsageFlags bloom_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    engine->bloom_target_image   = create_render_target_attachment(engine, 0, bloom_format, bloom_target_extents, bloom_usage);
    engine->bloom_pingpong_image = create_render_target_attachment(engine, 0, bloom_format, bloom_target_extents, bloom_usage);

    VERBOSE_LOG("- created render targets\n");

    

    // Graphics Pipeline
    //
    // NOTE: Only recreate graphics pipeline if format changed (no need to recreate it if only the extents changed).
    // Dynamic pipeline state let us handle the extent change (e.g. vkCmdSetViewport() and vkCmdSetScissor())
    // So we basically never recreate the graphics pipeline.
    //
    bool create_or_recreate_graphics_pipeline = (old_format != chosen_format.format);  // Format changed.
    if (create_or_recreate_graphics_pipeline)
    {
        vkDeviceWaitIdle(engine->device);
        
        destroy_all_graphics_pipelines(engine);
        create_all_graphics_pipelines(engine);
    }


    // Alloc postprocess descriptors and write to them
    engine->postprocess_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(engine->render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->render_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    engine->postprocess_pingpong_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(engine->render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->render_target_pingpong_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->postprocess_pingpong_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    engine->bloom_target_postprocess_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(engine->render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->bloom_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->bloom_target_postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    engine->bloom_pingpong_postprocess_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->postprocess_set_layout);
    {
        VkWriteDescriptorSet descriptors[POSTPROCESS_DESCRIPTOR_COUNT] = {};
        assert(engine->bloom_pingpong_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->bloom_pingpong_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->bloom_pingpong_postprocess_descriptor_set; 
        descriptors[0].dstBinding      = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    engine->bloom_apply_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->bloom_apply_set_layout);
    {
        VkWriteDescriptorSet descriptors[BLOOM_APPLY_DESCRIPTOR_COUNT] = {};
        assert(engine->render_target_image.image);
        assert(engine->bloom_target_image.image);

        VkDescriptorImageInfo scene_image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->render_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo bloom_image_info = {
            .sampler     = engine->postprocess_sampler,
            .imageView   = engine->bloom_target_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->bloom_apply_descriptor_set; 
        descriptors[0].dstBinding      = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_SCENE_TEXTURE; 
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &scene_image_info;

        descriptors[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[1].pNext           = NULL;
        descriptors[1].dstSet          = engine->bloom_apply_descriptor_set; 
        descriptors[1].dstBinding      = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_BLOOM_TEXTURE; 
        descriptors[1].descriptorCount = 1; 
        descriptors[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[1].pImageInfo      = &bloom_image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    // Alloc GBuffer descriptor sets and write to them
    engine->gbuffers_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->gbuffers_set_layout);
    {
        VkWriteDescriptorSet descriptors[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
        VkDescriptorImageInfo image_infos[RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT] = {};
        for (int i = 0; i < RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT; ++i)
        {
            GPU_Image* attachment;
            if (i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT)
                attachment = &engine->render_target_deferred_attachments[i];
            else
                attachment = &engine->render_target_depth;
            assert(attachment->image_view);

            image_infos[i] = {
                .sampler     = engine->postprocess_sampler,
                .imageView   = attachment->image_view,
                .imageLayout = i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };

            descriptors[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[i].pNext           = NULL;
            descriptors[i].dstSet          = engine->gbuffers_descriptor_set; 
            descriptors[i].dstBinding      = GBUFFERS_DESCRIPTOR_SET_BINDING_ALBEDO_ROUGHNESS + i;
            descriptors[i].descriptorCount = 1; 
            descriptors[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptors[i].pImageInfo      = &image_infos[i];
        }
        
        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }

    // Alloc Shadow map descriptor sets and write to them
    engine->shadow_maps_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->shadow_maps_set_layout);
    {
        VkWriteDescriptorSet descriptors[1] = {};
        assert(engine->render_target_image.image_view);
        VkDescriptorImageInfo image_info = {
            .sampler     = engine->shadow_map_sampler,
            .imageView   = engine->shadow_map_depth.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };

        descriptors[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].pNext           = NULL;
        descriptors[0].dstSet          = engine->shadow_maps_descriptor_set; 
        descriptors[0].dstBinding      = SHADOW_MAPS_DESCRIPTOR_SET_BINDING;
        descriptors[0].descriptorCount = 1; 
        descriptors[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors[0].pImageInfo      = &image_info;

        // Update descriptor sets
        const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
        vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
    }
}

void
destroy_swapchain(VulkanEngine* engine, VkSwapchainKHR swapchain)
{
    engine->is_a_swapchain_created = 0;

    // Destroy render targets
    destroy_image(engine, engine->render_target_image);
    destroy_image(engine, engine->render_target_pingpong_image);
    destroy_image(engine, engine->render_target_depth);

    // Destroy G Buffers
    for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
        destroy_image(engine, engine->render_target_deferred_attachments[i]);

    // Shadow map and bloom target
    destroy_image(engine, engine->shadow_map_depth);
    destroy_image(engine, engine->bloom_target_image);
    destroy_image(engine, engine->bloom_pingpong_image);

    // Destroy Image Views and Semaphores
    for (u32 i = 0; i < engine->swapchain_image_count; ++i)
    {
        vkDestroyImageView(engine->device, engine->swapchain_image_views[i], NULL);

        vkDestroySemaphore(engine->device, engine->swapchain_image_acquired_semaphores[i], NULL);
        vkDestroySemaphore(engine->device, engine->swapchain_image_render_semaphores[i], NULL);
    }
    L_free(engine->swapchain_image_views, &engine->main.tt);
    L_free(engine->swapchain_image_acquired_semaphores, &engine->main.tt);
    L_free(engine->swapchain_image_render_semaphores, &engine->main.tt);

    // Swapchain images are automatically destroyed when swapchain is destroyed due to ownership
    L_free(engine->swapchain_images, &engine->main.tt);

    vkDestroySwapchainKHR(engine->device, swapchain, NULL);
}


VkDescriptorPool
create_descriptor_pool(VulkanEngine* engine, u32 max_descriptors, u32 max_sets)
{
    // List of the kinds of descriptors that the pool should contain
    VkDescriptorPoolSize const pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_descriptors },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_descriptors },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_descriptors }
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = max_sets,
        .poolSizeCount = sizeof(pool_sizes)/sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(engine->device, &pool_create_info, NULL, &pool));

    return pool;
}

VkDescriptorSet
alloc_descriptor_set(VulkanEngine* engine, VkDescriptorPool pool, VkDescriptorSetLayout set_layout)
{
    VkDescriptorSetAllocateInfo set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout
    };

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(engine->device, &set_alloc_info, &descriptor_set));
    
    return descriptor_set;
}


void
create_all_descriptor_set_layouts(VulkanEngine* engine)
{
    // Allocate a large pool of descriptors and descriptor sets so we don't run out
    engine->descriptor_pool = create_descriptor_pool(engine, 2048, 1024);
    

    // Scene descriptor set layout
    {
        // Create uniform buffer for scene
        engine->scene_uniform_buffer = create_buffer(
            engine->vma_allocator,
            sizeof(SceneUniform_GLSL_ScalarBlock),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        // Set bindings for descriptor set layouts
        VkDescriptorSetLayoutBinding bindings[1] = {
            {
                .binding         = SCENE_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = SCENE_DESCRIPTOR_COUNT,
                .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_GEOMETRY_BIT,
                .pImmutableSamplers = NULL
            }
        };

        // Create scene set layout
        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .bindingCount = sizeof(bindings) / sizeof(bindings[0]),
            .pBindings = bindings
        };

        engine->scene_set_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->scene_set_layout));


        // Alloc scene descriptors and write to it
        engine->scene_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->scene_set_layout);
        {
            VkWriteDescriptorSet descriptors[SCENE_DESCRIPTOR_COUNT] = {};

            // Scene uniforms
            VkDescriptorBufferInfo scene_ubo_info = {
                .buffer = engine->scene_uniform_buffer.buffer,
                .offset = 0,
                .range = sizeof(SceneUniform_GLSL_ScalarBlock)
            };
            descriptors[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptors[0].pNext = NULL;
            descriptors[0].dstSet           = engine->scene_descriptor_set;
            descriptors[0].dstBinding       = SCENE_DESCRIPTOR_SET_BINDING_UNIFORMS;
            descriptors[0].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            descriptors[0].descriptorCount  = SCENE_DESCRIPTOR_COUNT;  // We bind each descriptor to a single binding point
            descriptors[0].pBufferInfo      = &scene_ubo_info;

            // Update descriptor sets for the scene
            // NOTE: (only a single uniform buffer for scene right now)
            const u32 num_sets = sizeof(descriptors) / sizeof(descriptors[0]);
            vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);
        }
    }

    // Create object descriptor set layout
    {
        // TODO: Small refactor so that the assert checking isn't run time (aka the static assert way I use for the G-Buffers)
        VkDescriptorSetLayoutBinding bindings[OBJECT_DESCRIPTOR_COUNT] = {};

        u32 bindings_implemented = 0;
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 0) uniform sampler2D rgba_albedo_alpha_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_ALBEDO_ALPHA_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 1) uniform sampler2D rgb_roughness_metalness_ao_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_RMA_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 2) uniform sampler2D rgb_normal_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_NORMAL_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };
        bindings[bindings_implemented++] = {
        // layout (set = 1, binding = 3) uniform sampler2D rgb_emissive_map;
            .binding          = OBJECT_DESCRIPTOR_SET_BINDING_EMISSIVE_MAP,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .stageFlags       = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };

        // Error checking: makes it easier to debug if we add more but forget to specify them in the layout here.
        assert(bindings_implemented == OBJECT_DESCRIPTOR_COUNT);

        // Create object set layout
        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding),
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->object_set_layout));
    }

    // Create postprocess descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = POSTPROCESS_DESCRIPTOR_SET_BINDING_SAMPLER,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = POSTPROCESS_DESCRIPTOR_COUNT,
            .pBindings     = bindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->postprocess_set_layout));

        // Create postprocess sampler
        {
            VkSamplerCreateInfo sampler_create_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
            #if 0  // FOR MOSAIC FILTER
                .magFilter         = VK_FILTER_NEAREST,
                .minFilter         = VK_FILTER_NEAREST, 
                .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            #else  // FOR BLOOM WE WANT LINEAR INSTEAD, of course this means the mosaic is no longer correct at the moment, but it's fine. The whole thing needs replacing with a simpler rendergraph system at some point anyway.
                .magFilter         = VK_FILTER_LINEAR,
                .minFilter         = VK_FILTER_LINEAR,
                .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            #endif
                .addressModeW      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .mipLodBias        = 0.0f,
                .anisotropyEnable  = VK_FALSE,
                .maxAnisotropy     = 0.0f,
                .compareEnable     = VK_FALSE,
                .compareOp         = VK_COMPARE_OP_NEVER,
                .minLod            = 0.0f,
                .maxLod            = VK_LOD_CLAMP_NONE,
                .borderColor       = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                .unnormalizedCoordinates = VK_FALSE
            };

            engine->postprocess_sampler = VK_NULL_HANDLE;
            VK_CHECK(vkCreateSampler(engine->device, &sampler_create_info, NULL, &engine->postprocess_sampler));
        }
    }

    // Create Bloom Apply descriptor set layout
    // NOTE: For now, just use postprocess_sampler with it, when writing descriptor sets
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_SCENE_TEXTURE,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = BLOOM_APPLY_DESCRIPTOR_SET_BINDING_BLOOM_TEXTURE,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = BLOOM_APPLY_DESCRIPTOR_COUNT,
            .pBindings     = bindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->bloom_apply_set_layout));
    }

    // Create G-Buffer samplers descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_ALBEDO_ROUGHNESS,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_NORMAL_METALNESS,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_EMISSIVE_AO,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            },
            {
                .binding         = GBUFFERS_DESCRIPTOR_SET_BINDING_DEPTH,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };
        static_assert(sizeof(bindings)/sizeof(bindings[0]) == RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT);

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = RENDER_TARGET_DEFERRED_ATTACHMENT_COUNT,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->gbuffers_set_layout));

        // NOTE: Currently using the postprocess sampler for GBuffer reads as well.
    }

    // Create lights descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = LIGHTS_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = 1,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->lights_set_layout));
    }

    // Create shadow maps descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = SHADOW_MAPS_DESCRIPTOR_SET_BINDING,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL
            }
        };

        VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = NULL,
            .bindingCount  = 1,
            .pBindings     = bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layout_create_info, NULL, &engine->shadow_maps_set_layout));

        // Create shadow map sampler
        VkSamplerCreateInfo sampler_create_info = {  // TODO: What sampling options should i use for shadows
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .magFilter         = VK_FILTER_LINEAR,  // Hardware PCF (Percentage closer filtering) when linear is combined with compareOp VK_TRUE
            .minFilter         = VK_FILTER_LINEAR,
            .mipmapMode        = VK_SAMPLER_MIPMAP_MODE_NEAREST,

            // Must clamp to border opaque white so that stuff outside the shadow map is considered unshadowed.
            .addressModeU      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW      = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias        = 0.0f,
            .anisotropyEnable  = VK_FALSE,
            .maxAnisotropy     = 0.0f,

            // Hardware shadow comparison
            .compareEnable     = VK_TRUE,
            .compareOp         = VK_COMPARE_OP_LESS_OR_EQUAL,  // LESS because not using reverse-z right now

            .minLod            = 0.0f,
            .maxLod            = VK_LOD_CLAMP_NONE,
            .borderColor       = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE
        };

        engine->shadow_map_sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(engine->device, &sampler_create_info, NULL, &engine->shadow_map_sampler));
    }
}

void
destroy_all_descriptor_set_layouts(VulkanEngine* engine)
{
    // Destroy lights descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->lights_set_layout, NULL);
    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        destroy_buffer(engine->vma_allocator, &engine->frames[i].lights_buffer);
    }

    // Destroy scene set layout and uniform buffer
    vkDestroyDescriptorSetLayout(engine->device, engine->scene_set_layout, NULL);
    vmaDestroyBuffer(engine->vma_allocator, engine->scene_uniform_buffer.buffer, engine->scene_uniform_buffer.allocation);

    // Destroy object descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->object_set_layout, NULL);

    // Destroy post process descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->postprocess_set_layout, NULL);
    vkDestroySampler(engine->device, engine->postprocess_sampler, NULL);

    // Destroy bloom descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->bloom_apply_set_layout, NULL);

    // Destroy gbuffers descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->gbuffers_set_layout, NULL);   

    // Destroy shadow map descriptor set layout
    vkDestroyDescriptorSetLayout(engine->device, engine->shadow_maps_set_layout, NULL);
    vkDestroySampler(engine->device, engine->shadow_map_sampler, NULL);



    // Destroy descriptor pool
    vkDestroyDescriptorPool(engine->device, engine->descriptor_pool, NULL);
}

Renderable
make_renderable_with_descriptors(VulkanEngine* engine, b32 is_opaque, GPU_MeshBuffers* mesh_ref, glm::mat4 initial_transform, ObjectDescriptorSetCreateInfo object_set_create_info)
{
    Renderable renderable = {};
    // renderable.valid_pipelines = valid_pipelines;
    renderable.is_opaque = is_opaque;
    renderable.mesh_ref = mesh_ref;
    renderable.initial_transform = initial_transform;


    // Alloc object descriptor set
    renderable.object_descriptor_set = alloc_descriptor_set(engine, engine->descriptor_pool, engine->object_set_layout);
    

    // Create the image info structs needed for writing
    VkDescriptorImageInfo texture_map_infos[OBJECT_TEXTURE_MAP_COUNT];
    for (u32 i = 0; i < OBJECT_TEXTURE_MAP_COUNT; ++i)
    {
        texture_map_infos[i] = {
            .sampler      = object_set_create_info.samplers[i],
            .imageView    = object_set_create_info.map_refs[i]->image_view,
            .imageLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    VkWriteDescriptorSet descriptors[OBJECT_DESCRIPTOR_COUNT] = {}; 
    const u32 num_sets = OBJECT_DESCRIPTOR_COUNT;

    // Write texture descriptors
    for (u32 i = 0; i < OBJECT_TEXTURE_MAP_COUNT; ++i)
    {
        const u32 texture_binding_start = OBJECT_DESCRIPTOR_SET_BINDING_ALBEDO_ALPHA_MAP;
        const u32 texture_map_binding_i = texture_binding_start + i;

        descriptors[i] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet            = renderable.object_descriptor_set,
            .dstBinding        = texture_map_binding_i,
            .dstArrayElement   = 0,
            .descriptorCount   = 1,
            .descriptorType    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo        = &texture_map_infos[i],
            .pBufferInfo       = NULL,
            .pTexelBufferView  = NULL
        };
    }

    // ... write other (non-texture) descriptors (none at the moment)

    // Update descriptor sets for this object
    vkUpdateDescriptorSets(engine->device, num_sets, descriptors, 0, NULL);

    return renderable;
}

GraphicsPipeline
create_graphics_pipeline(VulkanEngine* engine, GraphicsPipelineConfigInfo config)
{
    assert(engine->render_target_image.image != VK_NULL_HANDLE && "graphics pipeline relies on using the same image format as the render target image");

    GraphicsPipeline gfx = {
        .is_created = 1,
        .layout = VK_NULL_HANDLE,
        .pipeline = VK_NULL_HANDLE,
        .has_attrib = {}
    };
    for (u32 i = 0; i < sizeof(gfx.has_attrib) / sizeof(gfx.has_attrib[0]); ++i)
        gfx.has_attrib[i] = config.has_attrib[i];

    // Create pipeline layout
    VK_CHECK(vkCreatePipelineLayout(engine->device, &config.pipeline_layout_create_info, NULL, &gfx.layout));

    // Load shader code spirv files
    // - Using VK_KHR_maintenance5 feature of Vulkan 1.4 to skip VkShaderModule and instead pass spirv directly to the pipeline object.

    u32 num_shaders = 0;  // To count how many shaders files are in config
    
    u64 vert_spirv_size = 0;
    u32* vert_spirv_code = NULL;
    if (config.vertex_spirv_config.spirv_path)
    {
        ++num_shaders;
        vert_spirv_code = (u32*)L_load_binary_file(config.vertex_spirv_config.spirv_path, &vert_spirv_size, &engine->main.tt);
        if (vert_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.vertex_spirv_config.spirv_path);
            exit(1);
        }
    }
    
    u64 frag_spirv_size = 0;
    u32* frag_spirv_code = NULL;
    if (config.fragment_spirv_config.spirv_path)
    {
        ++num_shaders;
        frag_spirv_code = (u32*)L_load_binary_file(config.fragment_spirv_config.spirv_path, &frag_spirv_size, &engine->main.tt);
        if (frag_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.fragment_spirv_config.spirv_path);
            exit(1);
        }
    }

    u64 geom_spirv_size = 0;
    u32* geom_spirv_code = NULL;
    if (config.geometry_spirv_config.spirv_path)
    {
        ++num_shaders;
        geom_spirv_code = (u32*)L_load_binary_file(config.geometry_spirv_config.spirv_path, &geom_spirv_size, &engine->main.tt);
        if (geom_spirv_code == NULL)
        {
            fprintf(stderr, "Error: Failed to load SPIR-V shader: %s\n", config.geometry_spirv_config.spirv_path);
            exit(1);
        }
    }
    
    // Vertex and fragment shaders are the minimum for a graphics pipeline
    if (!config.vertex_spirv_config.spirv_path || !config.fragment_spirv_config.spirv_path)
    {
        fprintf(stderr, "Missing vertex or fragment shader path in config: Those two are the minimum required for a graphics pipeline.\n");
        exit(1);
    }


    // Put spirv into shader module create infos
    // Pass that into shader stages via the pNext chain
    assert(num_shaders >= 2);

    VkShaderModuleCreateInfo* code = (VkShaderModuleCreateInfo*)L_calloc(num_shaders, sizeof(VkShaderModuleCreateInfo), &engine->main.tt);  // C++ lacking variable length arrays be like
    VkPipelineShaderStageCreateInfo* stages = (VkPipelineShaderStageCreateInfo*)L_calloc(num_shaders, sizeof(VkPipelineShaderStageCreateInfo), &engine->main.tt);

    u32 stage_count = 0;

    // Vertex shader module and stage
    const u32 spirv_vertex_index = stage_count++;
    code[spirv_vertex_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    code[spirv_vertex_index].pNext     = NULL;
    code[spirv_vertex_index].flags     = 0;
    code[spirv_vertex_index].codeSize  = vert_spirv_size;
    code[spirv_vertex_index].pCode     = vert_spirv_code;

    stages[spirv_vertex_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[spirv_vertex_index].pNext   = &code[spirv_vertex_index];
    stages[spirv_vertex_index].flags   = 0;
    stages[spirv_vertex_index].stage   = VK_SHADER_STAGE_VERTEX_BIT;
    stages[spirv_vertex_index].module  = VK_NULL_HANDLE;
    stages[spirv_vertex_index].pName   = config.vertex_spirv_config.entrypoint_name;
    stages[spirv_vertex_index].pSpecializationInfo = config.vertex_spirv_config.pSpecializationInfo;

    // Geometry shader (Order of these shaders matters in the code and stages arrays, vertex->geometry->fragment)
    if (geom_spirv_code)
    {
        const u32 spirv_geometry_index = stage_count++;
        // Geometry shader module and stage
        code[spirv_geometry_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        code[spirv_geometry_index].pNext     = NULL;
        code[spirv_geometry_index].flags     = 0;
        code[spirv_geometry_index].codeSize  = geom_spirv_size;
        code[spirv_geometry_index].pCode     = geom_spirv_code;    

        stages[spirv_geometry_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[spirv_geometry_index].pNext   = &code[spirv_geometry_index];
        stages[spirv_geometry_index].flags   = 0;
        stages[spirv_geometry_index].stage   = VK_SHADER_STAGE_GEOMETRY_BIT;
        stages[spirv_geometry_index].module  = VK_NULL_HANDLE;
        stages[spirv_geometry_index].pName   = config.geometry_spirv_config.entrypoint_name;
        stages[spirv_geometry_index].pSpecializationInfo = config.geometry_spirv_config.pSpecializationInfo;
    }

    // Fragment shader module and stage
    const u32 spirv_fragment_index = stage_count++;
    code[spirv_fragment_index].sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    code[spirv_fragment_index].pNext     = NULL;
    code[spirv_fragment_index].flags     = 0;
    code[spirv_fragment_index].codeSize  = frag_spirv_size;
    code[spirv_fragment_index].pCode     = frag_spirv_code;    

    stages[spirv_fragment_index].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[spirv_fragment_index].pNext   = &code[spirv_fragment_index];
    stages[spirv_fragment_index].flags   = 0;
    stages[spirv_fragment_index].stage   = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[spirv_fragment_index].module  = VK_NULL_HANDLE;
    stages[spirv_fragment_index].pName   = config.fragment_spirv_config.entrypoint_name;
    stages[spirv_fragment_index].pSpecializationInfo = config.fragment_spirv_config.pSpecializationInfo;


    // Vertex attributes
    u32 attrib_count = 0;
    u32 binding_count = 0;
    VkVertexInputBindingDescription vertex_inputs[MAX_VERTEX_ATTRIBUTES];
    VkVertexInputAttributeDescription vertex_attributes[MAX_VERTEX_BINDINGS];
    {
        // TODO: For instanced rendering look into VK_VERTEX_INPUT_RATE_INSTANCE

        // Position (vec3f)
        if (config.has_attribute_position)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_POSITION;
            vertex_inputs[binding_count].stride    = sizeof(float) * 3;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_POSITION;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_POSITION;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Normal (vec3f)
        if (config.has_attribute_normal)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_NORMAL;
            vertex_inputs[binding_count].stride    = sizeof(float) * 3;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_NORMAL;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_NORMAL;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Texcoord (vec2f)
        if (config.has_attribute_texcoord)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_TEXCOORD;
            vertex_inputs[binding_count].stride    = sizeof(float) * 2;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_TEXCOORD;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_TEXCOORD;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }

        // Tangent (vec4f)
        if (config.has_attribute_tangent)
        {
            vertex_inputs[binding_count].binding   = ATTRIB_BINDING_TANGENT;
            vertex_inputs[binding_count].stride    = sizeof(float) * 4;
            vertex_inputs[binding_count].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++binding_count;

            vertex_attributes[attrib_count].binding   = ATTRIB_BINDING_TANGENT;
            vertex_attributes[attrib_count].location  = ATTRIB_LOC_TANGENT;
            vertex_attributes[attrib_count].format    = VK_FORMAT_R32G32B32A32_SFLOAT;
            vertex_attributes[attrib_count].offset    = 0;
            ++attrib_count;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.pNext = NULL;
    vertex_input_state_create_info.flags = 0;
    vertex_input_state_create_info.vertexBindingDescriptionCount    = binding_count;
    vertex_input_state_create_info.pVertexBindingDescriptions       = vertex_inputs;
    vertex_input_state_create_info.vertexAttributeDescriptionCount  = attrib_count;
    vertex_input_state_create_info.pVertexAttributeDescriptions     = vertex_attributes;

    // Input Assembly (the rasterization primitive)
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
    input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.pNext = NULL;
    input_assembly_create_info.flags = 0;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor regions
    VkViewport viewport = {};  // The framebuffer coords the ndcs map to
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)engine->render_target_extent.width;
    viewport.height = (float)engine->render_target_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};  // The subset of the framebuffer to actual rasterize to
    scissor.offset = (VkOffset2D){ 0, 0 };
    scissor.extent = engine->render_target_extent;

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.pNext = NULL;
    viewport_state_create_info.flags = 0;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.pViewports = &viewport;
    viewport_state_create_info.scissorCount = 1;
    viewport_state_create_info.pScissors = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo raster_state_create_info = {};
    raster_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_state_create_info.pNext = NULL;
    raster_state_create_info.flags = 0;
    raster_state_create_info.depthClampEnable = VK_FALSE;
    raster_state_create_info.rasterizerDiscardEnable = VK_FALSE;  // This would disable rasterization of our primitives so we definitely don't want that.
    // For example, there are things like Point-Tesselated Voxelization (Fei, et al. 2012) that use hardware tesselation but not use the rasterizer.
    raster_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_state_create_info.cullMode = config.cull_mode;
    raster_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_state_create_info.depthBiasEnable = VK_FALSE;
    raster_state_create_info.lineWidth = 1.0f;  // Required. (Because it only applies to line raster primitives or VK_POLYGON_MODE_LINE)
    
    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {};
    multisampling_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_state_create_info.pNext = NULL;
    multisampling_state_create_info.flags = 0;
    multisampling_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil buffer state
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .depthTestEnable        = VK_TRUE,
        .depthWriteEnable       = VK_TRUE,
        .depthCompareOp         = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable  = VK_FALSE,  // NOTE: I think depth bounds is for that CAT-scan like effect where you only see a portion of the depth range rendered.
        .stencilTestEnable      = VK_FALSE,  // NOTE: Not using stencil buffer at the moment
        .front                  = {},
        .back                   = {},
        .minDepthBounds         = 0.0f,
        .maxDepthBounds         = 1.0f
    };
    for (u32 i = 0; i < config.blend_mode_count; ++i)
    {
        if (config.blend_modes[i] == BLEND_MODE_ALPHA_BLEND)
        {
            // It's incorrect to write depth values of translucent fragments (except for BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH)
            // So if any color attachment using alpha blending we disable depth writing.
            depth_stencil_state_create_info.depthWriteEnable = VK_FALSE;
        }
    }

    // Color blend state (one per attachment)
    VkPipelineColorBlendAttachmentState* blend_states = (VkPipelineColorBlendAttachmentState*)L_calloc(config.blend_mode_count,  sizeof(VkPipelineColorBlendAttachmentState), &engine->main.tt);
    for (u32 i = 0; i < config.blend_mode_count; ++i)
    {
        BlendMode blend_mode = config.blend_modes[i];
        switch (blend_mode)
        {
            // NOTE: Not setting blend_states[i].alphaBlendOp. No purpose for it yet.

            case BLEND_MODE_OPAQUE:
            case BLEND_MODE_ALPHA_MASK:
                blend_states[i].blendEnable = VK_FALSE;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;
            
            case BLEND_MODE_ALPHA_BLEND:
            case BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH:
                blend_states[i].blendEnable          = VK_TRUE;
                blend_states[i].colorBlendOp         = VK_BLEND_OP_ADD;
                blend_states[i].srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_states[i].dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;

            case BLEND_MODE_ADD_TO_SRC:
                blend_states[i].blendEnable = VK_TRUE;
                blend_states[i].colorBlendOp = VK_BLEND_OP_ADD;
                blend_states[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_states[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_states[i].colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
                break;

            default:
                assert(0 && "Unimplemented blend mode.");
                abort();
        }
        // if (blend_mode == BLEND_MODE_ALPHA_BLEND || blend_mode == BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH)
        // {
        //     blend_states[i].blendEnable          = VK_TRUE;
        //     blend_states[i].colorBlendOp         = VK_BLEND_OP_ADD;
        //     blend_states[i].srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
        //     blend_states[i].dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        //     blend_states[i].colorWriteMask =
        //         VK_COLOR_COMPONENT_R_BIT |
        //         VK_COLOR_COMPONENT_G_BIT |
        //         VK_COLOR_COMPONENT_B_BIT |
        //         VK_COLOR_COMPONENT_A_BIT;
        // }
        // else 
        // {
        //     blend_states[i].blendEnable = VK_FALSE;
        //     blend_states[i].colorWriteMask =
        //         VK_COLOR_COMPONENT_R_BIT |
        //         VK_COLOR_COMPONENT_G_BIT |
        //         VK_COLOR_COMPONENT_B_BIT |
        //         VK_COLOR_COMPONENT_A_BIT;
        // }
    }
    
    VkPipelineColorBlendStateCreateInfo blend_state_create_info = {};
    blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state_create_info.pNext = NULL;
    blend_state_create_info.flags = 0;
    blend_state_create_info.logicOpEnable = VK_FALSE;
    blend_state_create_info.attachmentCount = config.blend_mode_count;
    blend_state_create_info.pAttachments = blend_states;

    // Dynamic States
    // Specifically scissor and viewport so we don't have to recreate the pipeline on every swapchain resize.
    // Although we'd still have to recreate the pipeline if the swapchain images change format (but that's much less frequent).
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.pNext = NULL;
    dynamic_state_create_info.flags = 0;

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    dynamic_state_create_info.dynamicStateCount = sizeof(dynamic_states)/sizeof(VkDynamicState);
    dynamic_state_create_info.pDynamicStates = dynamic_states;


    // Graphics Pipeline Create Info
    // All that shit goes into the graphics pipeline create info
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &config.pipeline_rendering_create_info,  // NOTE: pNext chain instead of Vulkan render passes.
        .flags = 0,
        .stageCount           = stage_count,
        .pStages              = stages,
        .pVertexInputState    = &vertex_input_state_create_info,
        .pInputAssemblyState  = &input_assembly_create_info,
        .pTessellationState   = NULL,  // TODO: Not using tesselation at the moment.
        .pViewportState       = &viewport_state_create_info,
        .pRasterizationState  = &raster_state_create_info,
        .pMultisampleState    = &multisampling_state_create_info,
        .pDepthStencilState   = &depth_stencil_state_create_info,
        .pColorBlendState     = &blend_state_create_info,
        .pDynamicState        = &dynamic_state_create_info,

        .layout = gfx.layout,

        // Not using render passes:
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &gfx.pipeline));


    L_free(blend_states, &engine->main.tt);

    // Free loaded shader files and allocated create infos
    if (vert_spirv_code) L_free(vert_spirv_code, &engine->main.tt);
    if (frag_spirv_code) L_free(frag_spirv_code, &engine->main.tt);
    if (geom_spirv_code) L_free(geom_spirv_code, &engine->main.tt);
    L_free(code, &engine->main.tt);
    L_free(stages, &engine->main.tt);


    VERBOSE_LOG("Graphics Pipeline Created.\n");

    return gfx;
}

void
destroy_graphics_pipeline(VulkanEngine* engine, GraphicsPipeline* gp)
{
    if (gp->is_created)
    {
        assert(gp->layout);
        assert(gp->pipeline);
        vkDestroyPipelineLayout(engine->device, gp->layout, NULL);
        vkDestroyPipeline(engine->device, gp->pipeline, NULL);
        
        *gp = {};
        gp->is_created = 0;  // Just being explicit
        gp->layout = VK_NULL_HANDLE;
        gp->pipeline = VK_NULL_HANDLE;
    }
}

static GraphicsPipeline
create_fullscreen_postprocess_pipeline(VulkanEngine* engine, const char* fragment_spirv_path, const VkFormat output_image_format)
{
    const char* vertex_spirv_path   = SPIRV_PATH_FULLSCREEN_VERT;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        engine->scene_set_layout,       // set 0
        engine->postprocess_set_layout  // set 1
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { output_image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    VERBOSE_LOG("Fullscreen postprocess ");
    return create_graphics_pipeline(engine, config);
}

static GraphicsPipeline
create_bloom_apply_pipeline(VulkanEngine* engine, VkFormat target_format)
{
    const char* vertex_spirv_path    = SPIRV_PATH_FULLSCREEN_VERT;
    const char* fragment_spirv_path  = SPIRV_PATH_BLOOM_APPLY_FRAG;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        engine->scene_set_layout,       // set 0
        engine->bloom_apply_set_layout  // set 1
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { target_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    VERBOSE_LOG("Fullscreen postprocess (apply bloom) ");
    return create_graphics_pipeline(engine, config);
}

static GraphicsPipeline
create_deferred_opaque_geometry_pipeline(VulkanEngine* engine)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_DEFERRED_WRITE_GBUFFERS_FRAG;

    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        engine->scene_set_layout,   // set 0
        engine->object_set_layout,  // set 1
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Specify G-Buffer attachments (except depth attachment which is specified seperately in VkPipelineRenderingCreateInfo):
    VkFormat color_formats[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
    for (u32 i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
    {
        color_formats[i] = engine->render_target_deferred_attachments[i].image_format;
    }

    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT] = {};
    for (int i = 0; i < RENDER_TARGET_DEFERRED_COLOR_ATTACHMENT_COUNT; ++i)
        blend_modes[i] = BLEND_MODE_OPAQUE;

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    VERBOSE_LOG("Deferred opaque geometry ");
    return create_graphics_pipeline(engine, config);
}

static GraphicsPipeline
create_deferred_lighting_pass_pipeline(VulkanEngine* engine)
{
    const char* vertex_spirv_path   = SPIRV_PATH_FULLSCREEN_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_DEFERRED_LIGHTING_FRAG;

    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        engine->scene_set_layout,       // set 0
        engine->gbuffers_set_layout,    // set 1
        engine->lights_set_layout,      // set 2
        engine->shadow_maps_set_layout  // set 3
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { engine->render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };

    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        // Fullscreen triangle has no vertex data as it's generated in the vertex shader.
        // See https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        .has_attribute_position  = 0,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_FRONT_BIT  // <- The procedural triangle is clockwise
    };
    
    VERBOSE_LOG("Deferred lighting ");
    return create_graphics_pipeline(engine, config);
}

static GraphicsPipeline
create_forward_opaque_pass(VulkanEngine* engine)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_FORWARD_LIGHTING_FRAG;

    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        engine->scene_set_layout,   // set 0
        engine->object_set_layout,  // set 1
        engine->lights_set_layout,  // set 2
        engine->shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { engine->render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { BLEND_MODE_OPAQUE };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    VERBOSE_LOG("Forward opaque ");
    return create_graphics_pipeline(engine, config);
}

static GraphicsPipeline
create_forward_transparent_pass(VulkanEngine* engine)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_FORWARD_LIGHTING_FRAG;

    // Pass specialization constants
    b32 is_alpha_masked = true;
    VkSpecializationMapEntry map_entry_is_alpha_masked = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(VkBool32)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_is_alpha_masked,
        .dataSize       = sizeof(VkBool32),
        .pData          = &is_alpha_masked
    };
    
    // TEMP: For the leaf sway....
    VkSpecializationInfo vertex_specialization_info = fragment_specialization_info;

    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        engine->scene_set_layout,   // set 0
        engine->object_set_layout,  // set 1
        engine->lights_set_layout,  // set 2
        engine->shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    const VkFormat color_formats[] = { engine->render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { BLEND_MODE_ALPHA_MASK };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &vertex_specialization_info
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_NONE  // Disable backface culling for alpha-masked leaves
    };

    VERBOSE_LOG("Forward transparent ");
    return create_graphics_pipeline(engine, config);
}

static void
create_general_debug_visuals_pipelines(VulkanEngine* engine)
{
    // Debug rendermodes:
    VERBOSE_LOG("Debug Viz %s\n", get_render_mode_name(engine->render_mode));

    const char* vertex_spirv_path    = SPIRV_PATH_SCENE_VERT;
    const char* fragment_spirv_path  = SPIRV_PATH_DEBUG_VISUALS_FRAG;

    // NOTE: Google's GLSLC doesn't support custom GLSL entrypoints so we do this by hand with a specialization constant for now.
    s32 fragment_entrypoint_id = 0;
    VkSpecializationMapEntry map_entry_fragment_entrypoint_id = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(fragment_entrypoint_id)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_fragment_entrypoint_id,
        .dataSize       = sizeof(fragment_entrypoint_id),
        .pData          = &fragment_entrypoint_id
    };

    BlendMode blend_mode_opaque_geometry = BLEND_MODE_OPAQUE;
    BlendMode blend_mode_transparent_geometry = BLEND_MODE_OPAQUE;

    // Select shader and graphics pipeline parameters based on render mode
    switch (engine->render_mode)
    {
        case RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_MIPLEVELS;
            break;

        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH;
            break;

        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_FRAGDEPTH_DERIVATIVES;
            break;
        
        case RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASELINE_OVERDRAW;

            blend_mode_opaque_geometry = BLEND_MODE_ALPHA_BLEND;
            blend_mode_transparent_geometry = BLEND_MODE_ALPHA_BLEND;
            break;

        case RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING:
            fragment_entrypoint_id = SHADER_ENTRYPOINT_ID_DEBUGVIZ_BASIC_OVERSHADING;

            blend_mode_opaque_geometry = BLEND_MODE_ALPHA_BLEND_BUT_WRITE_DEPTH;  // Write the depth just like how our opaque geometry is tested against in the lighting shaders
            blend_mode_transparent_geometry = BLEND_MODE_ALPHA_BLEND;             // Depth testing does not happen in our transparent frag shader so we maintain that here.
            break;

        default:
            fprintf(stderr, "Error: Unimplemented general debug render mode: %d\n", engine->render_mode);
            assert(0 && "Unimplemented render mode");
            exit(1);
    }

    
    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        engine->scene_set_layout,   // set 0
        engine->object_set_layout,  // set 1
        engine->lights_set_layout,  // set 2
        engine->shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { engine->render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { blend_mode_opaque_geometry };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 1,
        .has_attribute_texcoord  = 1,
        .has_attribute_tangent   = 1,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    VERBOSE_LOG("[\n  Opaque ");
    engine->gp_opaque_pass = create_graphics_pipeline(engine, config);

    VERBOSE_LOG("  Transparent ");
    config.blend_modes[0] = blend_mode_transparent_geometry;
    engine->gp_transparent_pass = create_graphics_pipeline(engine, config);
    VERBOSE_LOG("]\n");
}

static void
create_mesh_density_visualisation_pipelines(VulkanEngine* engine)
{
    // Debug rendermodes:
    VERBOSE_LOG("Debug Viz %s\n", get_render_mode_name(engine->render_mode));

    const char* vertex_spirv_path    = SPIRV_PATH_DEBUG_MESH_DENSITY_VERT;
    const char* geometry_spirv_path  = SPIRV_PATH_DEBUG_MESH_DENSITY_GEOM;
    const char* fragment_spirv_path  = SPIRV_PATH_DEBUG_MESH_DENSITY_FRAG;

    BlendMode blend_mode_opaque_geometry = BLEND_MODE_OPAQUE;
    BlendMode blend_mode_transparent_geometry = BLEND_MODE_OPAQUE;
    
    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        engine->scene_set_layout,       // set 0
        engine->gbuffers_set_layout,    // set 1
        engine->lights_set_layout,      // set 2
        engine->shadow_maps_set_layout  // set 3
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // Pipeline rendering info. Part of the dynamic rendering feature (core since Vulkan 1.3)
    // We don't specify a renderpass and instead use this struct in the pNext chain of VkGraphicsPipelineCreateInfo
    const VkFormat color_formats[] = { engine->render_target_image.image_format };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = sizeof(color_formats) / sizeof(VkFormat),
        .pColorAttachmentFormats  = color_formats,
        .depthAttachmentFormat    = engine->render_target_depth.image_format,
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_modes[] = { blend_mode_opaque_geometry };

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },
        .geometry_spirv_config = {
            .spirv_path          = geometry_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = NULL
        },

        .has_attribute_position  = 1,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 0,
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = sizeof(blend_modes) / sizeof(blend_modes[0]),
        .blend_modes = blend_modes,
        .cull_mode = VK_CULL_MODE_BACK_BIT
    };
    
    VERBOSE_LOG("[\n  Opaque ");
    engine->gp_opaque_pass = create_graphics_pipeline(engine, config);

    VERBOSE_LOG("  Transparent ");
    config.blend_modes[0] = blend_mode_transparent_geometry;
    engine->gp_transparent_pass = create_graphics_pipeline(engine, config);
    VERBOSE_LOG("]\n");
}

static GraphicsPipeline
create_shader_mapping_pipeline(VulkanEngine* engine, b32 is_transparent)
{
    const char* vertex_spirv_path   = SPIRV_PATH_SHADOW_MAP_VERT;
    const char* fragment_spirv_path = SPIRV_PATH_SHADOW_MAP_FRAG;

    // Pass specialization constants
    b32 is_alpha_masked = is_transparent;
    VkSpecializationMapEntry map_entry_is_alpha_masked = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(VkBool32)
    };
    VkSpecializationInfo fragment_specialization_info = {
        .mapEntryCount  = 1,
        .pMapEntries    = &map_entry_is_alpha_masked,
        .dataSize       = sizeof(VkBool32),
        .pData          = &is_alpha_masked
    };
    
    // TEMP: To capture the leaf sway in the shadow mapping of alpha masked folliage
    VkSpecializationInfo vertex_specialization_info = fragment_specialization_info;

    // Define which descripter set layouts the shaders use
    assert(engine->scene_set_layout != VK_NULL_HANDLE);
    assert(engine->object_set_layout != VK_NULL_HANDLE);
    VkDescriptorSetLayout layouts[] = {
        // Order must match the layout set value in the shaders
        // (which matches the constants in the shader info header)
        engine->scene_set_layout,  // set 0
        engine->object_set_layout  // set 1
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(Object_GLSL_PushConstants)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .setLayoutCount         = sizeof(layouts) / sizeof(VkDescriptorSetLayout),
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range
    };
    
    // No color formats
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType                    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                    = NULL,
        .viewMask                 = 0,
        .colorAttachmentCount     = 0,
        .pColorAttachmentFormats  = NULL,
        .depthAttachmentFormat    = engine->shadow_map_depth.image_format,  // TODO: When using multiple shadow maps, they should all be the same format so do: shadow_maps[0].format prolly
        .stencilAttachmentFormat  = VK_FORMAT_UNDEFINED
    };
    
    BlendMode blend_mode = is_transparent ? BLEND_MODE_ALPHA_MASK : BLEND_MODE_OPAQUE;

    GraphicsPipelineConfigInfo config = {
        .vertex_spirv_config = {
            .spirv_path          = vertex_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &vertex_specialization_info
        },
        .fragment_spirv_config = {
            .spirv_path          = fragment_spirv_path,
            .entrypoint_name     = "main",
            .pSpecializationInfo = &fragment_specialization_info
        },
        .other_shaders_spirv_configs = {},

        .has_attribute_position  = 1,
        .has_attribute_normal    = 0,
        .has_attribute_texcoord  = 1,  // NOTE: Texcoords only needed for alpha masked pipeline, but we include it in opaque as well so we don't have to write to seperate shader files
        .has_attribute_tangent   = 0,

        .pipeline_layout_create_info    = pipeline_layout_create_info,
        .pipeline_rendering_create_info = pipeline_rendering_create_info,
        .blend_mode_count = 1,
        .blend_modes = &blend_mode,
        .cull_mode = is_transparent ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT  // Disable backface culling for alpha-masked leaves
    };

    VERBOSE_LOG("Shadow map %s ", is_transparent ? "transparent" : "opaque");
    return create_graphics_pipeline(engine, config);
}

void
create_all_graphics_pipelines(VulkanEngine* engine)
{
    VERBOSE_LOG("Creating all graphics pipelines:\n");

    // Main geometry and lighting pipelines depends on rendermode:
    switch (engine->render_mode)
    {
        case RENDERMODE_DEFAULT_DEFERRED:
            engine->gp_opaque_pass = create_deferred_opaque_geometry_pipeline(engine);
            engine->gp_deferred_lighting = create_deferred_lighting_pass_pipeline(engine);
            engine->gp_transparent_pass = create_forward_transparent_pass(engine);
            break;
        
        case RENDERMODE_DEFAULT_FORWARD:
            engine->gp_opaque_pass = create_forward_opaque_pass(engine);
            engine->gp_transparent_pass = create_forward_transparent_pass(engine);
            break;
        
        case RENDERMODE_DEBUGVIZ_MESH_DENSITY:
            create_mesh_density_visualisation_pipelines(engine);
            break;
        
        case RENDERMODE_DEBUGVIZ_SHOW_MIPLEVELS:
        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH:
        case RENDERMODE_DEBUGVIZ_FRAGMENT_DEPTH_PARTIAL_DERIVATIVES:        
        case RENDERMODE_DEBUGVIZ_BASELINE_OVERDRAW:
        case RENDERMODE_DEBUGVIZ_BASIC_OVERSHADING:
            create_general_debug_visuals_pipelines(engine);
            break;

        default:
            fprintf(stderr, "Error: Unimplemented render mode: %d\n", engine->render_mode);
            assert(0 && "Unimplemented render mode");
            exit(1);
    }

    // Postprocess shaders
    engine->gp_fullscreen_tri_mosaic = create_fullscreen_postprocess_pipeline(engine, SPIRV_PATH_POSTPROCESS_MOSAIC_FRAG, engine->swapchain_image_format);

    // Bloom shaders (horizontal goes into an identical pingpong image buffer because we can't read and write to the same image at the same time during parallel computation)
    engine->gp_bloom_brightness_extraction    = create_fullscreen_postprocess_pipeline(engine, SPIRV_PATH_BLOOM_EXTRACT_BRIGHTNESS_FRAG, engine->bloom_target_image.image_format);
    engine->gp_bloom_gaussian_blur_horizontal = create_fullscreen_postprocess_pipeline(engine, SPIRV_PATH_BLOOM_HBLUR_FRAG, engine->bloom_pingpong_image.image_format);
    engine->gp_bloom_gaussian_blur_vertical   = create_fullscreen_postprocess_pipeline(engine, SPIRV_PATH_BLOOM_VBLUR_FRAG, engine->bloom_target_image.image_format);
    engine->gp_bloom_apply = create_bloom_apply_pipeline(engine, engine->render_target_pingpong_image.image_format);

    // Shadow mapping shaders
    engine->gp_shadowmap_opaque      = create_shader_mapping_pipeline(engine, 0);
    engine->gp_shadowmap_transparent = create_shader_mapping_pipeline(engine, 1);
}

void
destroy_all_graphics_pipelines(VulkanEngine* engine)
{
    destroy_graphics_pipeline(engine, &engine->gp_opaque_pass);
    destroy_graphics_pipeline(engine, &engine->gp_deferred_lighting);
    destroy_graphics_pipeline(engine, &engine->gp_transparent_pass);
    destroy_graphics_pipeline(engine, &engine->gp_fullscreen_tri_mosaic);
    destroy_graphics_pipeline(engine, &engine->gp_shadowmap_opaque);
    destroy_graphics_pipeline(engine, &engine->gp_shadowmap_transparent);
    destroy_graphics_pipeline(engine, &engine->gp_bloom_brightness_extraction);
    destroy_graphics_pipeline(engine, &engine->gp_bloom_gaussian_blur_horizontal);
    destroy_graphics_pipeline(engine, &engine->gp_bloom_gaussian_blur_vertical);
    destroy_graphics_pipeline(engine, &engine->gp_bloom_apply);
}

GPU_Buffer
create_buffer(VmaAllocator vma_allocator, u64 size, VkBufferUsageFlags buffer_usage_flags, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage)
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

void
destroy_buffer(VmaAllocator vma_allocator, const GPU_Buffer* gpu_buffer)
{
    vmaDestroyBuffer(vma_allocator, gpu_buffer->buffer, gpu_buffer->allocation);
}

VkCommandBuffer
begin_one_time_command(VulkanEngine* engine)
{
    // Reset the one time command pool
    VK_CHECK(vkResetCommandPool(engine->device, engine->onetime_command_pool, 0));
    VK_CHECK(vkResetFences(engine->device, 1, &engine->onetime_command_complete_fence));

    // Begin recording
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    VK_CHECK(vkBeginCommandBuffer(engine->onetime_command, &begin_info));

    // NOTE: No need to return currently since we know we are using engine->onetime_comand,
    // but if we change this to use more than one command buffer
    // this will make the refactoring easier later, so use the returned value.
    return engine->onetime_command;
}

void
end_one_time_command_and_wait(VulkanEngine* engine, VkCommandBuffer command)
{
    // NOTE: For now, we just use one pool and one command for this, so assert that...
    assert(command == engine->onetime_command);

    VK_CHECK(vkEndCommandBuffer(engine->onetime_command));

    // Submit commands
    VkCommandBufferSubmitInfo command_submit_info = vklayer_command_buffer_submit_info(engine->onetime_command);
    VkSubmitInfo2 submit = vklayer_submit_info(&command_submit_info, NULL, NULL);
    VK_CHECK(vkQueueSubmit2(engine->graphics_queue, 1, &submit, engine->onetime_command_complete_fence));

    // CPU waits for queued command to finish by waiting on the command complete fence
    VK_CHECK(vkWaitForFences(engine->device, 1, &engine->onetime_command_complete_fence, VK_TRUE, UINT64_MAX));
}

GPU_Buffer
create_staging_buffer_from_data(VmaAllocator vma_allocator, u8* data, u64 size)
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


GPU_MeshBuffers
create_mesh_buffers(VulkanEngine* engine,
    const u32*   indices,   u64 indices_size,    // sizes are in bytes
    const float* positions, u64 positions_size,
    const float* normals,   u64 normals_size,
    const float* texcoords, u64 texcoords_size,
    const float* tangents,  u64 tangents_size
)
{
    // To put a vertex attribute's data into a vertex attribute buffer, we use a mapped portion of memory
    // (aka a "staging buffer") and memcpy the data into that first.
    // Then we can copy that staging buffer into the VRAM buffer with a command submitted to a transfer queue.
    // (We could skip the staging buffer on integrated GPUs, but this works universally).



    // One buffer per vertex attribute (the non interleaved way of doing vertex data)
    const float* attributes[MAX_VERTEX_ATTRIBUTES] = { positions, normals, texcoords, tangents };
    u64 attribute_sizes[MAX_VERTEX_ATTRIBUTES] = { positions_size, normals_size, texcoords_size, tangents_size };

    // Create buffers for the mesh in video memory
    GPU_MeshBuffers mesh_buffers = {};
    mesh_buffers.index_count = indices_size / sizeof(u32);              // NOTE: Inferring sizes based off index and position attributes
    mesh_buffers.vertex_count = positions_size / (3 * sizeof(float));   // 

    // Buffer flags
    // NOTE: Specifying TRANSFER_DST flag because we will copy into it from a staging buffer
    VkBufferUsageFlags       common_buffer_usages = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateFlags allocation_flags     = 0;
    VmaMemoryUsage           memory_usage         = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    
    // Create index buffer
    mesh_buffers.index_buffer = create_buffer(
        engine->vma_allocator, indices_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | common_buffer_usages, allocation_flags, memory_usage
    );

    // Create buffer per attribute (position, normal, etc...)
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        mesh_buffers.attribute_buffers[i] = create_buffer(
            engine->vma_allocator, attribute_sizes[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | common_buffer_usages, allocation_flags, memory_usage
        );
    }


    // Staging Buffers with the mesh data copied into them.
    GPU_Buffer index_staging_buffer = create_staging_buffer_from_data(engine->vma_allocator, (u8*)indices,   indices_size);
    
    GPU_Buffer attribute_staging_buffers[MAX_VERTEX_ATTRIBUTES];
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        attribute_staging_buffers[i] = create_staging_buffer_from_data(engine->vma_allocator, (u8*)(attributes[i]), attribute_sizes[i]);
    }


    // Transfer from staging buffer to mesh buffers in video memory
    VkCommandBuffer transfer_command = begin_one_time_command(engine);


    // Define copy regions for copy commands
    VkBufferCopy index_copy_region     = { .srcOffset=0, .dstOffset=0, .size=indices_size   };
    VkBufferCopy attribute_copy_regions[MAX_VERTEX_ATTRIBUTES];
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        attribute_copy_regions[i] = { .srcOffset=0, .dstOffset=0, .size=attribute_sizes[i] };
    }


    // Record the copy commands to each buffer
    vkCmdCopyBuffer(transfer_command, index_staging_buffer.buffer,    mesh_buffers.index_buffer.buffer,    1, &index_copy_region);
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        vkCmdCopyBuffer(transfer_command, attribute_staging_buffers[i].buffer, mesh_buffers.attribute_buffers[i].buffer, 1, &attribute_copy_regions[i]);
    }

    
    // Barriers for commands
    const int barrier_count = 1 + MAX_VERTEX_ATTRIBUTES;  // (+1 for index buffer)
    VkBufferMemoryBarrier2 read_barriers[barrier_count];
    VkBufferMemoryBarrier2* index_read_barrier = &read_barriers[0];
    VkBufferMemoryBarrier2* attribute_read_barriers = &read_barriers[1];

    // Index data barrier
    *index_read_barrier = vklayer_specify_buffer_barrier(
        mesh_buffers.index_buffer.buffer, indices_size, 0,
        /* Before */
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,      // The result of the transfer write must be available before...
        VK_QUEUE_FAMILY_IGNORED,

        /* After */
        VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,  
        VK_ACCESS_2_INDEX_READ_BIT,          // ...the index buffer will be read from as index data.
        VK_QUEUE_FAMILY_IGNORED
    );

    // Vertex data barriers
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        attribute_read_barriers[i] = vklayer_specify_buffer_barrier(
            mesh_buffers.attribute_buffers[i].buffer, attribute_sizes[i], 0,
            /* Before */
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,         // The result of the transfer write must be available before...
            VK_QUEUE_FAMILY_IGNORED,

            /* After */
            VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
            VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,  // ...the vertex attributes will be read from as vertex attribute data.
            VK_QUEUE_FAMILY_IGNORED
        );
    }

    // Record the pipeline barriers so that the buffers are copied before they can be used for vertex shader input.
    vklayer_cmd_pipeline_barrier_for_buffers(transfer_command, barrier_count, read_barriers);

    end_one_time_command_and_wait(engine, transfer_command);

    destroy_buffer(engine->vma_allocator, &index_staging_buffer);
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        destroy_buffer(engine->vma_allocator, &attribute_staging_buffers[i]);
    }


    VERBOSE_LOG("Mesh buffers loaded into video memory.\n");

    // Return GPU buffers for index and vertex data that have been transferred.
    return mesh_buffers;
}

void
destroy_mesh_buffers(VulkanEngine* engine, GPU_MeshBuffers* mesh_buffers)
{
    destroy_buffer(engine->vma_allocator, &mesh_buffers->index_buffer);
    for (u32 i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
    {
        destroy_buffer(engine->vma_allocator, &mesh_buffers->attribute_buffers[i]);
    }
}


GPU_Image
create_attachment_image(
    VulkanEngine* engine,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect_flags,
    b32 has_msaa)
{
    // Images used for render attachments in a graphics pipeline/renderpass.

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
        assert(0 && "Not implemented MSAA attachment images yet");
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
    VK_CHECK(vmaCreateImage(engine->vma_allocator, &image_create_info, &alloc_create_info, &gpu_image.image, &gpu_image.allocation, NULL));


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
    VK_CHECK(vkCreateImageView(engine->device, &image_view_create_info, NULL, &gpu_image.image_view));
    
    return gpu_image;
}

GPU_Image
create_render_target_attachment(VulkanEngine* engine, b32 is_depth_attachment, VkFormat desired_format, VkExtent3D extent, VkImageUsageFlags usage)
{
    VkFormat format;
    // VkImageUsageFlags usage;
    VkImageAspectFlags aspect_flags;

    // NOTE: If is_depth_attachment, desired_format must be VK_FORMAT_UNDEFINED since we find whatever the best format based on the device
    if (is_depth_attachment)
    {
        assert(
            desired_format == VK_FORMAT_UNDEFINED &&
            "For depth, we find the best format automatically, so desired_format arg should be VK_FORMAT_UNDEFINED."
        );
    }

    if (is_depth_attachment)
    {
        // Get best depth format for this device
        format = vklayer_find_supported_depth_format(engine->physical_device);
        // usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // Include the depth aspect.
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
        
        // TODO: Check for stencil format and | with VK_IMAGE_ASPECT_STENCIL_BIT
    }
    else  // Color attachment
    {
        format = desired_format;

        // usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        //         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        //         VK_IMAGE_USAGE_SAMPLED_BIT;  // <- Last one is for postprocessing
        
        aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    return create_attachment_image(engine, extent, format, usage, aspect_flags, 0);  // No MSAA
}

GPU_Image
create_image_texture2d(VulkanEngine* engine, u8* data, u64 data_size, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage)
{
    // Uploads data to video memory via a staging buffer.
    // Always generates mipmaps.

    const u32 mip_levels = compute_num_mip_levels(width, height);
    VkExtent3D extent = { width, height, 1 };

    // NOTE: We require these to upload image data.
    assert(usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT && usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.flags = 0;
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateImage(engine->vma_allocator, &image_create_info, &alloc_create_info, &image, &allocation, NULL));


    // Load image data into video memory via staging buffer.
    GPU_Buffer staging_buffer = create_staging_buffer_from_data(engine->vma_allocator, data, data_size);
    VkCommandBuffer transfer_command = begin_one_time_command(engine);
    {
        // Transition whole image layout to TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier2 image_as_transfer_dst_barrier = vklayer_specify_image_transition_barrier(
            image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
            /* Before*/
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &image_as_transfer_dst_barrier);

        // Copy staging buffer to image base mip level (level 0)
        VkImageSubresourceLayers image_subresource_layers = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkBufferImageCopy copy = {
            .bufferOffset       = 0,
            .bufferRowLength    = 0,
            .bufferImageHeight  = 0,
            .imageSubresource   = image_subresource_layers,
            .imageOffset        = { 0, 0, 0 },
            .imageExtent        = { width, height, 1 }
        };
        vkCmdCopyBufferToImage(transfer_command, staging_buffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        // Now the base mip level has the image data, create the rest of the mip levels with blits
        // (Blits allow downsampling).

        // First transition base mip level to TRANSFER_SRC_OPTIMAL
        // (We already transitioned the remaining mip levels to TRANSFER_DST_OPTIMAL with the last image barrier)
        VkImageSubresourceRange base_subimage = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel    = 0,
            .levelCount      = 1,
            .baseArrayLayer  = 0,
            .layerCount      = 1
        };
        VkImageMemoryBarrier2 baselevel_to_transfer_src_barrier = vklayer_specify_image_transition_barrier(
            image, base_subimage,
            /* Before */
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &baselevel_to_transfer_src_barrier);

        // Process all mip levels with blits
        // Each subsequent blit copies at half of previous size.

        s32 current_width  = (s32)width;
        s32 current_height = (s32)height;
        // NOTE: Signed ints because thats what VkOffset3D uses.

        for (u32 level = 1; level < mip_levels; ++level)
        {
            // Blit previous mipmap level (level-1) to the current level.

            // Source is the previous mip level
            VkImageBlit blit_regions = {};
            blit_regions.srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level-1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            blit_regions.srcOffsets[0] = { 0, 0, 0 };
            blit_regions.srcOffsets[1] = { current_width, current_height, 1 };

            // Next mip level is half the size
            current_width  /= 2;
            current_height /= 2;

            // Dest is the current mip level
            blit_regions.dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            blit_regions.dstOffsets[0] = { 0, 0, 0 };
            blit_regions.dstOffsets[1] = { current_width, current_height, 1 };
            
            // Blit command:
            vkCmdBlitImage(transfer_command,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit_regions,
                VK_FILTER_LINEAR  // Linear filter is required for the mip mapping's averaging effect
            );

            // Transition this mip level to be a TRANSFER_SRC for the next iteration.
            // NOTE: We technically don't need to transfer the last mip level, but it simplifies
            // the final image barrier if the whole image ends up in the same format after this loop.
            VkImageSubresourceRange miplevel_i_subimage = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = level,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            VkImageMemoryBarrier2 miplevel_to_src_barrier = vklayer_specify_image_transition_barrier(
                image, miplevel_i_subimage,
                /* Before */
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                /* After */
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED
            );
            vklayer_cmd_transition_images(transfer_command, 1, &miplevel_to_src_barrier);
        }

        // Finally, now that all mip levels are filled with image data
        // Transition the whole image into format to be read by the fragment shader
        VkImageMemoryBarrier2 make_image_shader_read_ready_barrier = vklayer_specify_image_transition_barrier(
            image, vklayer_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
            /* Before*/
            VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_transition_images(transfer_command, 1, &make_image_shader_read_ready_barrier);

    }
    end_one_time_command_and_wait(engine, transfer_command);

    // Clean up staging buffer
    destroy_buffer(engine->vma_allocator, &staging_buffer);


    // Create image view
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VkImageView image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(engine->device, &image_view_create_info, NULL, &image_view));

    GPU_Image gpu_image = {
        .image = image,
        .image_view = image_view,
        .allocation = allocation,
        .image_extent = extent,
        .image_format = format
    };

    return gpu_image;
}

void
destroy_image(VulkanEngine* engine, GPU_Image gpu_image)
{
    assert(engine);
    assert(gpu_image.image != VK_NULL_HANDLE);
    assert(gpu_image.image_view != VK_NULL_HANDLE);

    vkDestroyImageView(engine->device, gpu_image.image_view, NULL);
    vmaDestroyImage(engine->vma_allocator, gpu_image.image, gpu_image.allocation);
}

// Deprecated: I don't load image files directly into textures,
// Instead I pack things like the roughness and metalness textures into one texture
// And same with base color and alpha, etc...
[[deprecated("I don't load image files directly into textures, i combine textures, make a load_rma textures, etc..")]]
GPU_Image
load_image_texture2d(VulkanEngine* engine, const char* filepath)
{
    // Load image file
    stbi_set_flip_vertically_on_load(1);
    int width, height, num_channels;
    u8* data = stbi_load(filepath, &width, &height, &num_channels, 4);  // Requesting 4 channels for RGBA
    if (data == NULL)
    {
        fprintf(stderr, "Failure to load image (%s)\n", filepath);
        exit(1);  // TODO: Could fallback to some default texture, but think about whether that should be a pointer, shallow, or deep copy.
    }


    u64 data_size = width * height * 4;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    GPU_Image loaded_image = create_image_texture2d(engine, data, data_size, width, height, format, usage);

    stbi_image_free(data);

    return loaded_image;
}

// This version tracks the GPU_Image layout for us
void
transition_gpu_images(VkCommandBuffer cmd, u32 image_barrier_count, const VkImageMemoryBarrier2* image_barriers, GPU_Image** gpu_image_refs)
{
    vklayer_cmd_transition_images(cmd, image_barrier_count, image_barriers);

    // NOTE: If a barrier does not correspond to a GPU_Image, you can set that index to NULL in gpu_image_refs
    for (u32 i = 0; i < image_barrier_count; ++i)
    {
        if (gpu_image_refs[i] != NULL)
        {
            // Update current layout
            gpu_image_refs[i]->current_layout = image_barriers[i].newLayout;
        }
    }
}

glm::mat4
get_camera_view_matrix(glm::vec3 position, float pitch, float yaw)
{
    // Calculate view matrix based on camera position, pitch and yaw
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::rotate(view, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::translate(view, -position);

    return view;
}

glm::vec3
get_camera_direction_from_view(glm::mat4 view_matrix)
{
    glm::mat4 camera_world_matrix = glm::inverse(view_matrix);
    return glm::normalize(glm::vec3(camera_world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
}

SceneUniform_GLSL_ScalarBlock
calculate_shadow_uniforms(VulkanEngine* engine)
{
    // glm::mat4 view = get_camera_view_matrix(engine->camera_position, engine->camera_pitch, engine->camera_yaw);

    // TEMP Perma spotlight
    glm::vec3 position = glm::vec3(5.555835f, -1.859704f, -6.053357f);
    glm::vec3 direction = glm::normalize(glm::vec3(-0.291220f, 0.226342f, -0.929495f));
    // glm::vec3 position = engine->camera_position + glm::vec3(0.5f, -0.2f, 0.0f);
    // glm::vec3 direction = glm::normalize(get_camera_direction_from_view(view) + vec3(0.01));
    float angle = glm::radians(60.0f);
    SpotLight spotlight = make_spot_light(
        position,
        glm::vec4(1.0f, 1.0f, 1.0f, 250.0f),
        glm::vec4(direction, cosf(angle / 2.0f))
    );

    glm::mat4 light_view = glm::lookAtRH(position, position + direction, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = 1.0f;  // Square shadow map
    float z_near = 0.1f;
    float z_far  = 100.0f;
    glm::mat4 light_projection = glm::perspectiveRH_ZO(angle, aspect, z_near, z_far);
    light_projection[1][1] *= -1;  // Vulkan Flips Y
    glm::mat4 light_view_projection = light_projection * light_view;

    // NOTE: Other fields are zeroed, only view, projection and view_projection are set for shadow map shaders
    SceneUniform_GLSL_ScalarBlock scene_uniforms = {
        .view            =light_view,
        .projection      =light_projection,
        .view_projection =light_view_projection,

        .framebuffer_size={glm::ivec2(engine->shadow_map_depth.image_extent.width, engine->shadow_map_depth.image_extent.height)},
        .time = (float)glfwGetTime(),
        ._padding0 = 0.0f,

        // Zero the rest
        .camera_world_pos  = {},
        .camera_near_plane = {},
        .camera_far_plane  = {},
        ._padding1 = {},
        .clear_color = {},
        .scene_ambient_color = {},

        .spotlight = spotlight,
        .spotlight_view_projection = light_view_projection
    };

    return scene_uniforms;
}


SceneUniform_GLSL_ScalarBlock
calculate_scene_uniforms(VulkanEngine* engine, u32 width, u32 height)
{
    glm::mat4 view = get_camera_view_matrix(engine->camera_position, engine->camera_pitch, engine->camera_yaw);

    // TEMP Perma spotlight
    glm::vec3 position = glm::vec3(5.555835f, -1.859704f, -6.053357f);
    glm::vec3 direction = glm::normalize(glm::vec3(-0.291220f, 0.226342f, -0.929495f));
    // glm::vec3 position = engine->camera_position + glm::vec3(0.5f, -0.2f, 0.0f);
    // glm::vec3 direction = glm::normalize(get_camera_direction_from_view(view) + vec3(0.01));
    float angle = glm::radians(60.0f);
    SpotLight spotlight = make_spot_light(
        position,
        glm::vec4(1.0f, 1.0f, 1.0f, 250.0f),
        glm::vec4(direction, cosf(angle / 2.0f))
    );

    glm::mat4 light_view = glm::lookAtRH(position, position + direction, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = 1.0f;  // Square shadow map
    float z_near = 0.1f;
    float z_far  = 100.0f;
    glm::mat4 light_projection = glm::perspectiveRH_ZO(angle, aspect, z_near, z_far);
    light_projection[1][1] *= -1;  // Vulkan Flips Y
    glm::mat4 light_view_projection = light_projection * light_view;

    // For the main renderpass, convert the spotlight matrix from snorm ndcs to unorm.
    const glm::mat4 snorm_to_unorm_matrix(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,  // Z is already 0 to 1 in Vulkan, so we leave it
        0.5f, 0.5f, 0.0f, 1.0f
    );
    glm::mat4 spotlight_view_projection_unorm = snorm_to_unorm_matrix * light_view_projection;

    // glm::vec3 camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    // glm::vec3 camera_up = glm::vec3(0.0f, 1.0f,  0.0f);
    // view = glm::lookAtRH(engine->camera_position, engine->camera_position + camera_forward, camera_up) * view;

    float aspect_ratio = (float)width / (float)height;
    const float near_plane = 0.1;
    const float far_plane = 400.0;
    glm::mat4 projection = glm::perspectiveRH_ZO(engine->camera_fov, aspect_ratio, near_plane, far_plane);
    projection[1][1] *= -1;  // Vulkan Flips Y

    glm::mat4 view_projection = projection * view;
    
    SceneUniform_GLSL_ScalarBlock scene_uniforms = {
        .view=view,
        .projection=projection,
        .view_projection=view_projection,

        .framebuffer_size=glm::ivec2(engine->render_target_extent.width, engine->render_target_extent.height),
        .time = (float)glfwGetTime(),
        ._padding0 = 0.0f,

        .camera_world_pos  = glm::vec4(engine->camera_position.x, engine->camera_position.y, engine->camera_position.z, 0.0f),
        .camera_near_plane = near_plane,
        .camera_far_plane  = far_plane,
        ._padding1 = {},

        .clear_color = engine->clear_color,

        .scene_ambient_color = glm::vec4(0.01f, 0.01f, 0.01f, 0.0f),

        .spotlight = spotlight,
        .spotlight_view_projection = spotlight_view_projection_unorm
    };

    return scene_uniforms;
}

void
update_scene_uniforms(VulkanEngine* engine, VkCommandBuffer cmd, SceneUniform_GLSL_ScalarBlock scene_block)
{
    VkBufferMemoryBarrier2 upload_scene_uniforms_barrier = vklayer_specify_buffer_barrier(
            engine->scene_uniform_buffer.buffer, sizeof(SceneUniform_GLSL_ScalarBlock), 0,
            /* Before */
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,  // Make vertex stage of any previous render pass has completed...
            VK_ACCESS_2_UNIFORM_READ_BIT,           // ...reading of any uniform data.
                                                    // NOTE: Currently only reading scene uniforms in vertex shader, if fragment shader also does, bitwise or it with fragment stage 2 bit
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_CLEAR_BIT,          // vkCmdUpdateBuffer is a clear command
            VK_ACCESS_2_TRANSFER_WRITE_BIT,         // We will write the new uniform data to this bit of memory
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_pipeline_barrier_for_buffers(cmd, 1, &upload_scene_uniforms_barrier);

        // Update buffer with provided scene_block
        vkCmdUpdateBuffer(cmd, engine->scene_uniform_buffer.buffer, 0, sizeof(SceneUniform_GLSL_ScalarBlock), &scene_block);

        VkBufferMemoryBarrier2 finished_uploading_scene_uniforms_barrier = vklayer_specify_buffer_barrier(
            engine->scene_uniform_buffer.buffer, sizeof(SceneUniform_GLSL_ScalarBlock), 0,
            /* Before */
            VK_PIPELINE_STAGE_2_CLEAR_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            /* After */
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_UNIFORM_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED
        );
        vklayer_cmd_pipeline_barrier_for_buffers(cmd, 1, &finished_uploading_scene_uniforms_barrier);
}

void
create_draw_lists(VulkanEngine* engine, DrawCommandLists* draw_lists)
{
    for (u32 renderpass_id = 0; renderpass_id < RENDERPASS_INDEX_COUNT; ++renderpass_id)
    {
        draw_lists->capacities[renderpass_id] = DRAW_COMMAND_LISTS_STARTING_CAPACITY;
        draw_lists->sizes[renderpass_id] = 0;
        draw_lists->lists[renderpass_id] = (DrawCommandInfo*)L_calloc(DRAW_COMMAND_LISTS_STARTING_CAPACITY, sizeof(DrawCommandInfo), &engine->main.tt);
    }
}

void
destroy_draw_lists(VulkanEngine* engine, DrawCommandLists* draw_lists)
{
    for (u32 renderpass_id = 0; renderpass_id < RENDERPASS_INDEX_COUNT; ++renderpass_id)
    {
        L_free(draw_lists->lists[renderpass_id], &engine->main.tt);
    }
}

void
clear_draw_lists(DrawCommandLists* draw_lists)
{
    // Clears without reallocing so that memory allocations don't happen unless lots of new objects spawn in
    for (u32 renderpass_id = 0; renderpass_id < RENDERPASS_INDEX_COUNT; ++renderpass_id)
    {
        draw_lists->sizes[renderpass_id] = 0;
    }
}

void
draw_to_graphics_pipeline(VulkanEngine* engine, RenderpassIndex renderpass_id, DrawCommandInfo draw_command_info)
{
    DrawCommandLists* draw_lists = &engine->draw_lists;

    assert(renderpass_id >= 0 && renderpass_id < RENDERPASS_INDEX_COUNT);
    // assert(
    //     (draw_command_info.renderable_ref->valid_pipelines & (1 << renderpass_id)) &&
    //     "Renderable for draw call is not compatable with the provided graphics pipeline index."
    // );

    assert(
        draw_lists->lists[renderpass_id] != NULL &&
        "Draw lists not initialised. This should be done during vk_init()"
    );

    // Check if array needs more capacity:
    if (draw_lists->sizes[renderpass_id] >= draw_lists->capacities[renderpass_id])
    {
        // #warning L_realloc needs implementing
        // assert(0 && "Draw list needs more capacity but L_realloc not implemented yet\n");
        draw_lists->capacities[renderpass_id] *= 2;
        draw_lists->lists[renderpass_id] = (DrawCommandInfo*)L_realloc(draw_lists->lists[renderpass_id], draw_lists->capacities[renderpass_id], &engine->main.tt);
    }
    
    // Add to end of array and increment size to account for the new element
    draw_lists->lists[renderpass_id][draw_lists->sizes[renderpass_id]] = draw_command_info;
    ++draw_lists->sizes[renderpass_id];
}

void
record_renderpass(VulkanEngine* engine, VkCommandBuffer cmd, FrameData* current_frame, bool bind_lights,
    RenderpassIndex renderpass_id, const GraphicsPipeline* const gp, RenderingAttachments rendering_attachments)
{
    // NOTE: For shadows, just change the scene uniforms before this to render from a different perspective.

    //
    // SETUP RENDER PASS
    //

    // Pass attachment information to render_info so we can begin rendering    
    VkRenderingInfo render_info = {
        .sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext                 = NULL,
        .flags                 = 0,
        .renderArea            = rendering_attachments.render_area,
        .layerCount            = 1,
        .viewMask              = 0,
        .colorAttachmentCount  = rendering_attachments.color_attachment_count,
        .pColorAttachments     = rendering_attachments.color_attachments,
        .pDepthAttachment      = rendering_attachments.depth_attachment,
        .pStencilAttachment    = NULL  // TODO: Include stencil attachment
    };

    
    //
    // BEGIN RENDER PASS
    //

    vkCmdBeginRendering(cmd, &render_info);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->pipeline);

        // Dynamic pipeline states: Set viewport and scissor.
        vkCmdSetViewport(cmd, 0, 1, &rendering_attachments.viewport);
        vkCmdSetScissor(cmd, 0, 1, &rendering_attachments.scissor);

        
        // Bind descriptor set for Scene
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gp->layout,
            SCENE_DESCRIPTOR_SET_INDEX,
            1,
            &engine->scene_descriptor_set,
            0,
            NULL
        );

        if (bind_lights)  // TODO: Put which sets need to be bound as part of the GraphicsPipeline probably. Instead of this floating if statement.
        {
            // Bind descriptor set for the lights
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gp->layout,
                LIGHTS_DESCRIPTOR_SET_INDEX,
                1,
                &current_frame->lights_descriptor_set,
                0,
                NULL
            );

            // Bind descriptor set for the shadow map
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gp->layout,
                SHADOW_MAPS_DESCRIPTOR_SET_INDEX,
                1,
                &engine->shadow_maps_descriptor_set,
                0,
                NULL
            );
        }

        //
        // DRAW OBJECTS
        //

        for (u32 obj_i = 0; obj_i < engine->draw_lists.sizes[renderpass_id]; ++obj_i)
        {
            const DrawCommandInfo* draw_info = &engine->draw_lists.lists[renderpass_id][obj_i];

            // Object descriptors (fixed, defined by a Renderable)
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gp->layout,
                OBJECT_DESCRIPTOR_SET_INDEX,
                1,
                &draw_info->renderable_ref->object_descriptor_set,
                0,     // No dynamic offsets
                NULL   // ..................
            );

            // We get the actual model matrix by applying the renderables initial_transform before the draw commands model matrix
            glm::mat4 combined_model_transform = draw_info->model_matrix * draw_info->renderable_ref->initial_transform;

            // Push constants (varying each frame: model matrix)
            // TODO: If adding animation data, the push constants block will be too small
            //       Instead use an SSBO for all animation data, and use the push constants to give an offset into that buffer
            Object_GLSL_PushConstants push_constants = {
                .model = combined_model_transform
            };
            vkCmdPushConstants(cmd, gp->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Object_GLSL_PushConstants), &push_constants);

            // Get the mesh data this draw command wants to render with
            const GPU_MeshBuffers* mesh_buffers = draw_info->renderable_ref->mesh_ref;

            // Load the buffers for each attribute in the pipeline
            for (u32 b = 0; b < MAX_VERTEX_ATTRIBUTES; ++b)
            {
                if (gp->has_attrib[b])
                {
                    VkBuffer buffer = mesh_buffers->attribute_buffers[b].buffer;
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(cmd, b, 1, &buffer, &offset);
                }
            }
            vkCmdBindIndexBuffer(cmd, mesh_buffers->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(
                cmd,                        // commandBuffer
                mesh_buffers->index_count,  // indexCount
                1,                          // instanceCount  (TODO: Support instanced rendering, if need be)
                0,                          // firstIndex
                0,                          // vertexOffset
                0                           // firstInstance
            );
        }

    }
    vkCmdEndRendering(cmd);
}

// void
// gaussian_blur_vertical(VulkanEngine* engine, VkCommandBuffer cmd, GPU_Image bloom_src, GPU_Image bloom_dst)
// {
//     // TODO: Refactor into something more sane.
// }

// NOTE: No need for scene changing right now, so removing this.
// It's easy to get scene changing working when I need to.
// void
// queue_scene_change(VulkanEngine* engine, SceneInfo* new_scene)
// {
//     if (engine->is_a_scene_change_queued)
//         VERBOSE_LOG("Warning, scene change already queued.\n");
//     engine->is_a_scene_change_queued = 1;
//     engine->queued_next_scene = new_scene;
// }


// void
// alloc_and_load_empty_scene(VulkanEngine* engine, u32 num_statics)
// {
//     assert(!engine->is_scene_loaded && "You must explicitly handle destroying last scene before loading another one.");

//     // NOTE: Could easily use a dynamic array instead but num statics should be known on initialisation right?
//     SceneInfo scene = {};
//     scene.num_static_renderables = num_statics;
//     scene.static_renderables = (Renderable*)L_calloc(scene.num_static_renderables, sizeof(Renderable), &engine->main.tt);

//     // FUTURE: Alloc dynamic array for dynamic objects
//     // ...

//     engine->is_scene_loaded = 1;
//     engine->loaded_scene = scene;
// }

// void
// destroy_loaded_scene(VulkanEngine* engine)
// {
//     assert(engine->is_scene_loaded);

//     L_free(engine->loaded_scene.static_renderables, &engine->main.tt);
// }

