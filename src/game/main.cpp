#include "core/core.h"
#include "renderer/renderer.h"
#include "core/InputManager.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_gamepad.h"
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

    InputManager& input = InputManager::GetInstance();
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

            input.ProcessEvent(event);

            switch (event.type)
            {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    input.PrintKeyboardEvent(event.key);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    input.PrintMouseMotionEvent(event.motion);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    input.PrintMouseButtonEvent(event.button);
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    input.PrintMouseWheelEvent(event.wheel);
                    break;

                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    input.PrintGamepadButtonEvent(event.gbutton);
                    break;

                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    input.PrintGamepadAxisEvent(event.gaxis);
                    break;

                default:
                    break;
            }

            Renderer_ListenToWindowEvent(event);
        }

        // Update input state
        input.Update();
        input.UpdateGamepadState();

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

    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
