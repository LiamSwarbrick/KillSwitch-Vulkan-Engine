#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include "my_c_runtime.h"
#include "assetsys.h"
#include <stdbool.h>

#include "SDL3/SDL.h"

// TODO: Maybe put config file in init info?
typedef struct Core_InitInfo
{
    const char* title;
    uint32_t width;
    uint32_t height;
}
Core_InitInfo;

SDL_Window* Core_Init(Core_InitInfo init_info);
void Core_Shutdown(SDL_Window* window);

SDL_Window* Core_CreateEngineWindow(const char* title, int width, int height);
void Core_DestroyWindow();

// Currently plan to use opaque handles for referring to things
// from different modules maybe, e.g.:
// typedef uint32_t MeshHandle;
// typedef uint32_t TextureHandle;
// typedef uint32_t BufferHandle;

#endif  // ENGINE_CORE_H
