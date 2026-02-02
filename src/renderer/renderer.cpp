#include "renderer.h"
#include "render_state.h"

// TODO: Define internal renderer struct (in internal header) and define here:
// RenderState render_state;  // With extern render_state in other ones. 

RenderState renderstate;

bool Renderer_Init(const Renderer_InitInfo* info)
{
    // TODO: Initialize vulkan, storing the stuff from my old renderer into internal render state

    SDL_Log("TODO: Create engine.cpp and try to directly include as much of my vulkan renderer as possible in order to get up and running as quick as possible.\n");

    SDL_Log("Reminder to self: Get a basic cube renderer working, that takes a camera and list of positions as input i guess\n");

    return true;
}

void Renderer_Shutdown()
{

}

void Renderer_OnWindowResize()
{

}


