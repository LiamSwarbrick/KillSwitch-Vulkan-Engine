#ifndef RENDERER_DRAWCALL_H
#define RENDERER_DRAWCALL_H

#include "shaders.h"
#include "core/my_c_runtime.h"

typedef struct DrawCall
{
    Renderable* renderable;

    uint64_t   object_ptr;   // GPU Address of the glsl ObjectData struct (contains model matrix etc.)
    uint64_t   joints_ptr;   // GPU Address of the skinning matrices (0 if not skinned)
}
DrawCall;

#define MAX_DRAWCALLS_PER_SHADER 10000
typedef struct DrawCallsPerShader
{
    b32 is_allocated;
    b32 is_currently_adding_drawcalls;  // So we can make sure SubmitDraw is only called between Begin and EndDrawCalls

    // Seperate drawcall list for each shader type.
    // Renderpasses can loop over the lists of the specific shaders that pass uses only.
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

typedef int (*DrawPrimSortFunc)(const void*, const void*);
int DrawPrimSortFunc_Default(const void* a, const void* b);

void ResetDrawArena();
void PushDrawPrimitive(DrawCall dc, PipelineKey pipeline_key, uint32_t prim_idx, uint32_t sort_key);
void SortDraws(DrawPrimSortFunc sort_func);
void ExecuteDraws(VkCommandBuffer cmd, PushConstant_PassHeader push_pass, uint64_t scene_ptr);

void ExecuteFullscreenPass(VkCommandBuffer cmd, uint32_t shader_id, PipelineKey key, PushConstant_PassHeader push_pass, uint64_t scene_ptr);

#endif  // RENDERER_DRAWCALL_H
