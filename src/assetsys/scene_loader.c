#include "cgltf.h"
#include "assetsys.h"

// Global scene (for now)
static Scene* g_scene = NULL;

void load_scene(const char* filename) {
    // Free old scene if exists
    if (g_scene) {
        free_scene(g_scene);
    }

    g_scene = load_asset(filename);
}

Scene* get_current_scene() {
    return g_scene;
}

void unload_scene() {
    if (g_scene) {
        free_scene(g_scene);
        g_scene = NULL;
    }
}