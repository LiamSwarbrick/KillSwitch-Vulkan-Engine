#ifndef ASSETSYS_H
#define ASSETSYS_H

#include "cgltf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mesh {
    const char* name;

    // Vertex data
    float* positions;
    float* normals;
    float* texcoords;
    size_t vertex_count;

    // Index data
    uint32_t* indices;
    size_t index_count;

    int material_index;
} Mesh;

typedef struct Material {
    const char* name;

    // PBR properties
    float base_color[4]; //gltf uses RGBA
    float metallic;
    float roughness;

    // Texture indices (if any)
    int base_color_texture_index;
    int metallic_roughness_texture_index;
} Material;

typedef struct Scene {
    Mesh* meshes;
    size_t mesh_count;

    Material* materials;
    size_t material_count;

    cgltf_data* _internal_gltf_data;
} Scene;

Scene* load_asset(const char* filename);

void free_scene(Scene* scene);

#ifdef __cplusplus
}
#endif

#endif