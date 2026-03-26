#ifndef ASSETSYS_H
#define ASSETSYS_H

#include "cgltf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Primitive {
    // Vertex data
    float* positions;
    float* normals;
    float* texcoords;
    size_t vertex_count;

    // Index data
    uint32_t* indices;
    size_t index_count;

    int material_index;
} Primitive;

typedef struct Mesh {
    const char* name;

    Primitive* primitives;
    size_t primitive_count;
} Mesh;

typedef struct Material {
    const char* name;

    // PBR properties
    float base_color[4];  // RGBA
    float metallic;
    float roughness;
    float emissive_factor[3]; // RGB 
    float alpha_cutoff;      

    // Texture indices (-1 if not used)
    int base_color_texture_index;           // Albedo/Diffuse map
    int metallic_roughness_texture_index;   // Metal/Rough map
    int normal_map_texture_index;           // Normal map
    int emissive_texture_index;             // Emission/glow map
    int occlusion_texture_index;            // Ambient Occlusion map
    
} Material;

typedef struct Texture {
    const char* name;
    int image_index;

	//Samplers: could just make a sampler struct but for now this is fine
    int mag_filter;   
    int min_filter;   
    int wrap_s;        
    int wrap_t;
} Texture;

typedef struct Image {
    const char* name;
    const char* uri;           
    const unsigned char* data; 
    size_t data_size;
} Image;

typedef struct Camera {
    const char* name;
    int type; // 0 = perspective, 1 = orthographic

    // Perspective data
    float yfov;
    float znear;
    float zfar;

    //could do orthographic if needed
} Camera;

typedef struct Light {
    const char* name;
    int type; // 0 = directional, 1 = point, 2 = spot
    float color[3];
    float intensity;
    float range;
} Light;

typedef struct Node {
    const char* name;

    // Transform data
    float translation[3];
    float rotation[4]; // Quaternion
    float scale[3];
    float matrix[16];  //could be local transform matrix

    // Hierarchy
    int parent_index; // root = -1
    int* children_indices;
    size_t child_count;

    int mesh_index;
    int camera_index;
    int light_index;

    // IMPORTANT: Custom properties from Blender are stored here
    // cgltf stores this as raw JSON data in `node->extras`, 
    // we should copy it so the game can parse "room_name": "COURTYARD"
    char* extras_json;
} Node;

typedef struct AnimationSampler {
    float* inputs;
    size_t input_count;

    float* outputs;
    size_t output_count;

    // Interpolation type: 0 = Linear, 1 = Step, 2 = CubicSpline 
    int interpolation;
} AnimationSampler;

typedef struct AnimationChannel {
    int sampler_index;
    int target_node_index; // which node is moving

    // Path: 0 = translation, 1 = rotation, 2 = scale, 3 = weights
    int target_path;
} AnimationChannel;

typedef struct Animation {
    const char* name;

    AnimationSampler* samplers;
    size_t sampler_count;

    AnimationChannel* channels;
    size_t channel_count;
} Animation;

typedef struct Asset {

    Mesh* meshes;
    size_t mesh_count;

    Material* materials;
    size_t material_count;

    Texture* textures;
    size_t texture_count;

    Image* images;
    size_t image_count;

    Camera* cameras;
    size_t camera_count;

    Light* lights;
    size_t light_count;

    Node* nodes;
    size_t node_count;

    Animation* animations;
    size_t animation_count;

} Asset;


Asset* load_asset(const char* filename);

void free_asset(Asset* asset);

Asset** load_asset_folder(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif