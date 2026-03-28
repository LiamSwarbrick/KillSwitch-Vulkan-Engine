#ifndef RENDERER_DRAWCALL_H
#define RENDERER_DRAWCALL_H

#include "shaders.h"

typedef struct DrawCall
{
    Renderable* renderable;

    float      sort_depth;   // <-TODO unused (move this step to render pass execute callback).
    uint64_t   object_ptr;   // GPU Address of the glsl ObjectData struct (contains model matrix etc.)
    uint64_t   joints_ptr;   // GPU Address of the skinning matrices (0 if not skinned)
}
DrawCall;

#define MAX_DRAWCALLS_PER_SHADER 10000
typedef struct DrawCallsPerShader
{
    b32 is_allocated;
    b32 is_currently_adding_drawcalls;  // So we can make sure SubmitDraw is only called between Begin and EndDrawCalls
    struct
    {
        uint32_t drawcall_count;
        DrawCall* drawcalls;
    } array[SHADER_COUNT];
}
DrawCallsPerShader;

// Collect draw calls
void InitDrawCallCollections();
void DestroyDrawCallCollections();

void BeginDrawCalls();
void AddDrawCall(Renderable* r);
void EndDrawCalls();

// After draw calls collected, each renderpass will
// want to set their own scene data e.g.
// - custom camera matrices,
// - custom rendertarget size
// etc.
// So call this function within the renderpass execute callback
void UpdateGlobalSceneData(SceneData data);
void ExecuteDrawCall(VkCommandBuffer cmd, DrawCall drawcall, PipelineKey key);

#endif  // RENDERER_DRAWCALL_H
