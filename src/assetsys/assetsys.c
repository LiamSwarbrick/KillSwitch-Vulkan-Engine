#include "cgltf.h"
#include "assetsys.h"
#include "SDL3/SDL.h"
#include <stdio.h>
#include <stdlib.h>

void load_asset(const char* filename) {
	cgltf_options options = { 0 };
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse scene: %d\n", result);
		return;
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
		return;
	}

	result = cgltf_validate(data);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid glTF data: %d\n", result);
		cgltf_free(data);
		return;
	}

#ifndef NDEBUG
	SDL_Log("Scene loaded: %zu nodes, %zu meshes, %zu cameras, %zu lights\n",
		data->nodes_count, data->meshes_count, data->cameras_count, data->lights_count);

	if (data->meshes_count > 0) {
		cgltf_mesh* mesh = &data->meshes[0];
		SDL_Log("Mesh 0 (%s): %zu primitives\n",
			mesh->name ? mesh->name : "(unnamed)",
			mesh->primitives_count);

		for (size_t p = 0; p < mesh->primitives_count; p++) {
			cgltf_primitive* prim = &mesh->primitives[p];
			SDL_Log("    Primitive %zu:\n", p);
			SDL_Log("    Type: %d \n", prim->type);
			SDL_Log("    Attributes: %zu\n", prim->attributes_count);

			for (size_t i = 0; i < prim->attributes_count; i++) {
				cgltf_attribute* attr = &prim->attributes[i];
				const char* attr_name = "UNKNOWN";
				switch (attr->type) {
				case cgltf_attribute_type_position: attr_name = "POSITION"; break;
				case cgltf_attribute_type_normal: attr_name = "NORMAL"; break;
				case cgltf_attribute_type_tangent: attr_name = "TANGENT"; break;
				case cgltf_attribute_type_texcoord: attr_name = "TEXCOORD"; break;
				case cgltf_attribute_type_color: attr_name = "COLOR"; break;
				case cgltf_attribute_type_joints: attr_name = "JOINTS"; break;
				case cgltf_attribute_type_weights: attr_name = "WEIGHTS"; break;
				}
				cgltf_accessor* accessor = attr->data;
				SDL_Log("      %s: count=%zu, type=%d, component_type=%d\n",
					attr_name,
					accessor->count,
					accessor->type,
					accessor->component_type);
			}

			if (prim->material) {
				SDL_Log("    Material: %s\n",
					prim->material->name ? prim->material->name : "(unnamed)");
				SDL_Log("      Has PBR: %d\n", prim->material->has_pbr_metallic_roughness);
				SDL_Log("      Has base color texture: %d\n",
					prim->material->pbr_metallic_roughness.base_color_texture.texture != NULL);
			}
			else {
				SDL_Log("Material: none\n");
			}
		}
	}
#endif

	cgltf_free(data);
}
