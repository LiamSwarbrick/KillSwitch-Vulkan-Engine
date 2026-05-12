#include "core.h"

SDL_Window* Core_Init(Core_InitInfo init_info)
{
    bool success = SDL_Init(
        SDL_INIT_VIDEO | SDL_INIT_GAMEPAD
        // | SDL_INIT_AUDIO 
    );
    if (!success)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to init SDL\n");
        exit(1);
    }

    SDL_SetAppMetadata("Adventure Engine Game", "earlyprototype", NULL);
    
    SDL_Window* window = SDL_CreateWindow(init_info.title, init_info.width, init_info.height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_SetWindowFullscreen(window, true);
    SDL_Log("Core Initialized\n");
    return window;
}

void Core_Shutdown(SDL_Window* window)
{
    SDL_DestroyWindow(window);
    SDL_Quit();

    SDL_Log("Core Shutdown.\n");
}

