#include "core/core.h"
#include "renderer/renderer.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "core/resource_manager.h"

int main(int argc, char *argv[])
{
    bool is_debugging = true;
#ifdef NDEBUG
    is_debugging = false;
#endif

    SDL_Window* window = Core_Init((Core_InitInfo){
        "Close-quarters Adventure Game",
        1280, 720
    });

    Renderer_InitInfo renderer_info = { .window = window, .enable_validation = is_debugging };
    Renderer_Init(&renderer_info);

    ResourceManager resource_manager = ResourceManager_Create((ResourceManagerCreateInfo){
        .debug_name = "GameAssets",
        .initial_capacity = 32
    });

    ResourceRequestDesc boot_assets[] = {
        {
            .type = RESOURCE_TYPE_SHADER_BYTECODE,
            .residency = RESOURCE_RESIDENCY_BOOT,
            .logical_name = "shader.unlit.vert",
            .source_path = "shaderspv/unlit.vert.spv"
        },
        {
            .type = RESOURCE_TYPE_SHADER_BYTECODE,
            .residency = RESOURCE_RESIDENCY_BOOT,
            .logical_name = "shader.unlit.frag",
            .source_path = "shaderspv/unlit.frag.spv"
        }
    };

    ResourceHandle boot_handles[SDL_arraysize(boot_assets)] = {};
    ResourceManager_RequestBatch(
        &resource_manager,
        boot_assets,
        boot_handles,
        (u32)SDL_arraysize(boot_assets)
    );

    ResourceManager_LogSummary(&resource_manager);
    
    // Testing shader upload api.
    // const uint32_t* test_vert_spv = {};    
    // uint16_t shader_id = Renderer_RegisterShaders()

    bool running = true;
    while (running)
    {
        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;

            Renderer_ListenToWindowEvent(event);
        }

        // Game ticks
        // TODO:

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            // NOTE: Passes will gather their own renderables, reason being, some passes will just draw a hardcoded full screen triangle,
            // others will draw from the lights perspective, and others will only draw the toon shaded characters for example
            //
            // TODO: Gather entity renderables in RenderView, put this as a function in renderer/renderpasses/gather_renderables.cpp or something
            // Later TODO: Gather visible entities only for extra optimization.
            // with support for different passes e.g. shadows from lights perspective will require different entity lists.
            // In future, when entity system sorted out, can move entity gathering
            // to inside the Renderer_DrawFrame function, and this can use core
            // systems to gather relevant entities per each render pass.
            
            Renderer_DrawFrame();
        }
    }

    ResourceManager_UnloadByResidency(
        &resource_manager,
        RESOURCE_RESIDENCY_TRANSIENT,
        1
    );

    ResourceManager_Destroy(&resource_manager);

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;

}
