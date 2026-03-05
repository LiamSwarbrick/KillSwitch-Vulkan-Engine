#include "cgltf.h"
#include "assetsys.h"

#ifdef __cplusplus
extern "C" {
#endif

void load_scene(const char* filename);
Scene* get_current_scene();
void unload_scene();

#ifdef __cplusplus
}
#endif