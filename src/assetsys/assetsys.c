#include "cgltf.h"
#include "assetsys.h"
#include "SDL3/SDL.h"
#include <stdio.h>
#include <stdlib.h>

Scene* load_asset(const char* filename) {
	cgltf_options options = { 0 };
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse scene: %d\n", result);
		return NULL;
	}

#ifndef NDEBUG
	SDL_Log("Parsed successfully. Checking buffers...\n");
	SDL_Log("Number of buffers: %zu\n", data->buffers_count);
	for (size_t i = 0; i < data->buffers_count; i++) {
		SDL_Log("  Buffer %zu: uri='%s', size=%zu bytes\n",
			i,
			data->buffers[i].uri ? data->buffers[i].uri : "(embedded)",
			data->buffers[i].size);
	}
#endif

	result = cgltf_load_buffers(&options, data, filename);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load buffers: %d\n", result);
		cgltf_free(data);
		return NULL;
	}

	result = cgltf_validate(data);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid glTF data: %d\n", result);
		cgltf_free(data);
		return NULL;
	}

	// Allocate scene
	Scene* scene = (Scene*)calloc(1, sizeof(Scene));
	if (!scene) {
		cgltf_free(data);
		return NULL;
	}

	scene->_internal_gltf_data = data;
	scene->mesh_count = data->meshes_count;
	scene->material_count = data->materials_count;

	// Allocate arrays
	scene->meshes = (Mesh*)calloc(scene->mesh_count, sizeof(Mesh));
	scene->materials = (Material*)calloc(scene->material_count, sizeof(Material));

	// Copy mesh data
	for (size_t i = 0; i < scene->mesh_count; i++) {
		cgltf_mesh* gltf_mesh = &data->meshes[i];
		Mesh* mesh = &scene->meshes[i];
		mesh->name = gltf_mesh->name;

		//TODO: extract materials, vertex attributes, indices, etc.
	}

	return scene;
}

void free_scene(Scene* scene) {
	if (!scene) return;

	// Free mesh data
	for (size_t i = 0; i < scene->mesh_count; i++) {
		free(scene->meshes[i].positions);
		free(scene->meshes[i].normals);
		free(scene->meshes[i].texcoords);
		free(scene->meshes[i].indices);
	}
	free(scene->meshes);
	free(scene->materials);

	// Free cgltf data
	cgltf_free(scene->_internal_gltf_data);

	free(scene);
}
