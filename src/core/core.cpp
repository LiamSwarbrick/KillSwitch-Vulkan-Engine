#include "core.h"

bool Core_Init()
{
    bool success = SDL_Init(
        SDL_INIT_VIDEO
        // | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD
    );
    if (!success)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to init SDL\n");
        return false;
    }

    SDL_SetAppMetadata("Adventure Engine Game", "earlyprototype", NULL);
    
    SDL_Log("Core Initialized\n");
    return true;
}

void Core_Shutdown()
{
    SDL_Quit();

    SDL_Log("Core Shutdown.\n");
}

SDL_Window* Core_CreateEngineWindow(const char* title, int width, int height)
{
    SDL_Window* window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window: %s\n", SDL_GetError());
        return NULL;
    }

    return window;
}
