#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include "my_c_runtime.h"
#include <stdbool.h>

#include "SDL3/SDL.h"

typedef struct Core_InitInfo
{
    const char* title;
    uint32_t width;
    uint32_t height;
}
Core_InitInfo;

bool Core_Init();
void Core_Shutdown();

SDL_Window* Core_CreateEngineWindow(const char* title, int width, int height);
void Core_DestroyWindow();

// E.g. (this is my current plan)
typedef uint32_t MeshHandle;
typedef uint32_t TextureHandle;
typedef uint32_t BufferHandle;

#endif  // ENGINE_CORE_H
