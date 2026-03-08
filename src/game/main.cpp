#include "core/core.h"
#include "renderer/renderer.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

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
            RenderView render_view = {};
            // TODO: Gather entity renderables in RenderView
            // Later TODO: Gather visible entities only for extra optimization.
            
            retry_with_resized_window:
            if (!Renderer_DrawFrame(&render_view))
            {
                // Swapchain was out of date, so try again.
                goto retry_with_resized_window;
            }
        }
    }

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
