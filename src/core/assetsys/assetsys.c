#include "cgltf.h"
#include "assetsys.h"
#include "SDL3/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper functions to get indices from cgltf pointers
static int get_mesh_index(cgltf_data* data, cgltf_mesh* target) { return target ? (int)(target - data->meshes) : -1; }
static int get_camera_index(cgltf_data* data, cgltf_camera* target) { return target ? (int)(target - data->cameras) : -1; }
static int get_light_index(cgltf_data* data, cgltf_light* target) { return target ? (int)(target - data->lights) : -1; }
static int get_node_index(cgltf_data* data, cgltf_node* target) { return target ? (int)(target - data->nodes) : -1; }
static int get_material_index(cgltf_data* data, cgltf_material* target) { return target ? (int)(target - data->materials) : -1; }
static int get_texture_index(cgltf_data* data, cgltf_texture* target) { return target ? (int)(target - data->textures) : -1; }
static int get_image_index(cgltf_data* data, cgltf_image* target) { return target ? (int)(target - data->images) : -1; }
static int get_sampler_index(cgltf_animation* anim, cgltf_animation_sampler* target) { return target ? (int)(target - anim->samplers) : -1; }

// Helper for copying strings
static char* duplicate_string(const char* src) {
	if (!src) return NULL;
	size_t len = strlen(src) + 1;
	char* dst = (char*)malloc(len);
	if (dst) memcpy(dst, src, len);
	return dst;
}

Asset* load_asset(const char* filename) {
	cgltf_options options = { 0 };
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);
	if (result != cgltf_result_success) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse asset: %d\n", result);
		return NULL;
	}

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

	// Allocate asset memory
	Asset* asset = (Asset*)calloc(1, sizeof(Asset));
	if (!asset) {
		cgltf_free(data);
		return NULL;
	}

	// Set counts
	asset->mesh_count = data->meshes_count;
	asset->material_count = data->materials_count;
	asset->texture_count = data->textures_count;
	asset->image_count = data->images_count;
	asset->camera_count = data->cameras_count;
	asset->light_count = data->lights_count;
	asset->node_count = data->nodes_count;
	asset->animation_count = data->animations_count;

	// Allocate arrays
	if (asset->mesh_count > 0) asset->meshes = (Mesh*)calloc(asset->mesh_count, sizeof(Mesh));
	if (asset->material_count > 0) asset->materials = (Material*)calloc(asset->material_count, sizeof(Material));
	if (asset->texture_count > 0) asset->textures = (Texture*)calloc(asset->texture_count, sizeof(Texture));
	if (asset->image_count > 0) asset->images = (Image*)calloc(asset->image_count, sizeof(Image));
	if (asset->camera_count > 0) asset->cameras = (Camera*)calloc(asset->camera_count, sizeof(Camera));
	if (asset->light_count > 0) asset->lights = (Light*)calloc(asset->light_count, sizeof(Light));
	if (asset->node_count > 0) asset->nodes = (Node*)calloc(asset->node_count, sizeof(Node));
	if (asset->animation_count > 0) asset->animations = (Animation*)calloc(asset->animation_count, sizeof(Animation));

	// Copy Images
	for (size_t i = 0; i < asset->image_count; i++) {
		asset->images[i].name = duplicate_string(data->images[i].name);
		asset->images[i].uri = duplicate_string(data->images[i].uri);
	}

	// Copy Textures
	for (size_t i = 0; i < asset->texture_count; i++) {
		cgltf_texture* gltf_tex = &data->textures[i];
		Texture* tex = &asset->textures[i];

		tex->name = duplicate_string(gltf_tex->name);
		tex->image_index = get_image_index(data, gltf_tex->image);

		if (gltf_tex->sampler) {
			tex->mag_filter = gltf_tex->sampler->mag_filter;
			tex->min_filter = gltf_tex->sampler->min_filter;
			tex->wrap_s = gltf_tex->sampler->wrap_s;
			tex->wrap_t = gltf_tex->sampler->wrap_t;
		}
		else {
			// Set defaults in renderer
			tex->mag_filter = 0;
			tex->min_filter = 0;
			tex->wrap_s = 0;
			tex->wrap_t = 0;
		}
	}

	// Copy Materials
	for (size_t i = 0; i < asset->material_count; i++) {
		cgltf_material* gltf_mat = &data->materials[i];
		Material* mat = &asset->materials[i];

		mat->name = duplicate_string(gltf_mat->name);

		// Map textures
		mat->base_color_texture_index = gltf_mat->has_pbr_metallic_roughness && gltf_mat->pbr_metallic_roughness.base_color_texture.texture ? get_texture_index(data, gltf_mat->pbr_metallic_roughness.base_color_texture.texture) : -1;
		mat->metallic_roughness_texture_index = gltf_mat->has_pbr_metallic_roughness && gltf_mat->pbr_metallic_roughness.metallic_roughness_texture.texture ? get_texture_index(data, gltf_mat->pbr_metallic_roughness.metallic_roughness_texture.texture) : -1;
		mat->normal_map_texture_index = gltf_mat->normal_texture.texture ? get_texture_index(data, gltf_mat->normal_texture.texture) : -1;
		mat->emissive_texture_index = gltf_mat->emissive_texture.texture ? get_texture_index(data, gltf_mat->emissive_texture.texture) : -1;
		mat->occlusion_texture_index = gltf_mat->occlusion_texture.texture ? get_texture_index(data, gltf_mat->occlusion_texture.texture) : -1;

		if (gltf_mat->has_pbr_metallic_roughness) {
			memcpy(mat->base_color, gltf_mat->pbr_metallic_roughness.base_color_factor, sizeof(float) * 4);
			mat->metallic = gltf_mat->pbr_metallic_roughness.metallic_factor;
			mat->roughness = gltf_mat->pbr_metallic_roughness.roughness_factor;
		}
		else {
			mat->base_color[0] = mat->base_color[1] = mat->base_color[2] = mat->base_color[3] = 1.0f;
			mat->metallic = 1.0f;
			mat->roughness = 1.0f;
		}

		memcpy(mat->emissive_factor, gltf_mat->emissive_factor, sizeof(float) * 3);
		mat->alpha_cutoff = gltf_mat->alpha_cutoff;
	}

	// Copy Cameras
	for (size_t i = 0; i < asset->camera_count; i++) {
		cgltf_camera* gltf_cam = &data->cameras[i];
		Camera* cam = &asset->cameras[i];

		cam->name = duplicate_string(gltf_cam->name);
		cam->type = (gltf_cam->type == cgltf_camera_type_perspective) ? 0 : 1;

		if (gltf_cam->type == cgltf_camera_type_perspective) {
			cam->yfov = gltf_cam->data.perspective.yfov;
			cam->znear = gltf_cam->data.perspective.znear;
			cam->zfar = gltf_cam->data.perspective.zfar;
		}
	}

	// Copy Lights
	for (size_t i = 0; i < asset->light_count; i++) {
		cgltf_light* gltf_light = &data->lights[i];
		Light* light = &asset->lights[i];

		light->name = duplicate_string(gltf_light->name);
		// cgltf types: 1=directional, 2=point, 3=spot
		light->type = gltf_light->type;
		memcpy(light->color, gltf_light->color, sizeof(float) * 3);
		light->intensity = gltf_light->intensity;
		light->range = gltf_light->range;
	}

	// Copy Nodes
	for (size_t i = 0; i < asset->node_count; i++) {
		cgltf_node* gltf_node = &data->nodes[i];
		Node* node = &asset->nodes[i];

		node->name = duplicate_string(gltf_node->name);
		node->parent_index = get_node_index(data, gltf_node->parent);
		node->mesh_index = get_mesh_index(data, gltf_node->mesh);
		node->camera_index = get_camera_index(data, gltf_node->camera);
		node->light_index = get_light_index(data, gltf_node->light);

		memcpy(node->translation, gltf_node->translation, sizeof(float) * 3);
		memcpy(node->rotation, gltf_node->rotation, sizeof(float) * 4);
		memcpy(node->scale, gltf_node->scale, sizeof(float) * 3);
		if (gltf_node->has_matrix) {
			memcpy(node->matrix, gltf_node->matrix, sizeof(float) * 16);
		}

		if (gltf_node->children_count > 0) {
			node->child_count = gltf_node->children_count;
			node->children_indices = (int*)malloc(node->child_count * sizeof(int));
			for (size_t c = 0; c < node->child_count; c++) {
				node->children_indices[c] = get_node_index(data, gltf_node->children[c]);
			}
		}

		// Save custom blender properties for trigger zones/spawns
		if (gltf_node->extras.data) {
			node->extras_json = duplicate_string(gltf_node->extras.data);
		}
	}

	// Copy Meshes and Primitives
	for (size_t i = 0; i < asset->mesh_count; i++) {
		cgltf_mesh* gltf_mesh = &data->meshes[i];
		Mesh* mesh = &asset->meshes[i];

		mesh->name = duplicate_string(gltf_mesh->name);
		mesh->primitive_count = gltf_mesh->primitives_count;
		mesh->primitives = (Primitive*)calloc(mesh->primitive_count, sizeof(Primitive));

		for (size_t p = 0; p < mesh->primitive_count; p++) {
			cgltf_primitive* gltf_prim = &gltf_mesh->primitives[p];
			Primitive* prim = &mesh->primitives[p];

			prim->material_index = get_material_index(data, gltf_prim->material);

			if (gltf_prim->indices) {
				prim->index_count = gltf_prim->indices->count;
				prim->indices = (uint32_t*)malloc(prim->index_count * sizeof(uint32_t));
				for (size_t idx = 0; idx < prim->index_count; idx++) {
					prim->indices[idx] = (uint32_t)cgltf_accessor_read_index(gltf_prim->indices, idx);
				}
			}

			for (size_t a = 0; a < gltf_prim->attributes_count; a++) {
				cgltf_attribute* attr = &gltf_prim->attributes[a];

				if (attr->type == cgltf_attribute_type_position) {
					prim->vertex_count = attr->data->count;
					prim->positions = (float*)malloc(prim->vertex_count * 3 * sizeof(float));
					cgltf_accessor_unpack_floats(attr->data, prim->positions, prim->vertex_count * 3);
				}
				else if (attr->type == cgltf_attribute_type_normal) {
					prim->normals = (float*)malloc(attr->data->count * 3 * sizeof(float));
					cgltf_accessor_unpack_floats(attr->data, prim->normals, attr->data->count * 3);
				}
				else if (attr->type == cgltf_attribute_type_texcoord) {
					prim->texcoords = (float*)malloc(attr->data->count * 2 * sizeof(float));
					cgltf_accessor_unpack_floats(attr->data, prim->texcoords, attr->data->count * 2);
				}
			}
		}
	}

	// Copy Animations
	for (size_t i = 0; i < asset->animation_count; i++) {
		cgltf_animation* gltf_anim = &data->animations[i];
		Animation* anim = &asset->animations[i];

		anim->name = duplicate_string(gltf_anim->name);
		anim->sampler_count = gltf_anim->samplers_count;
		anim->channel_count = gltf_anim->channels_count;

		anim->samplers = (AnimationSampler*)calloc(anim->sampler_count, sizeof(AnimationSampler));
		anim->channels = (AnimationChannel*)calloc(anim->channel_count, sizeof(AnimationChannel));

		for (size_t s = 0; s < anim->sampler_count; s++) {
			cgltf_animation_sampler* gltf_samp = &gltf_anim->samplers[s];
			AnimationSampler* samp = &anim->samplers[s];

			samp->interpolation = gltf_samp->interpolation;

			// Input times
			if (gltf_samp->input) {
				samp->input_count = gltf_samp->input->count;

				samp->inputs = (float*)malloc(samp->input_count * sizeof(float));
				cgltf_accessor_unpack_floats(gltf_samp->input, samp->inputs, samp->input_count);
			}

			// Output values
			if (gltf_samp->output) {
				size_t num_components = cgltf_num_components(gltf_samp->output->type);
				samp->output_count = gltf_samp->output->count * num_components;

				samp->outputs = (float*)malloc(samp->output_count * sizeof(float));
				cgltf_accessor_unpack_floats(gltf_samp->output, samp->outputs, samp->output_count);
			}
		}

		for (size_t c = 0; c < anim->channel_count; c++) {
			cgltf_animation_channel* gltf_chan = &gltf_anim->channels[c];
			AnimationChannel* chan = &anim->channels[c];

			chan->sampler_index = get_sampler_index(gltf_anim, gltf_chan->sampler);
			chan->target_node_index = get_node_index(data, gltf_chan->target_node);

			// 0=translation, 1=rotation, 2=scale, 3=weights, 4=color
			if (gltf_chan->target_path == cgltf_animation_path_type_translation) chan->target_path = 0;
			else if (gltf_chan->target_path == cgltf_animation_path_type_rotation) chan->target_path = 1;
			else if (gltf_chan->target_path == cgltf_animation_path_type_scale) chan->target_path = 2;
			else if (gltf_chan->target_path == cgltf_animation_path_type_weights) chan->target_path = 3;
		}
	}

	cgltf_free(data);
	return asset;
}

void free_asset(Asset* asset) {
	if (!asset) return;

	for (size_t i = 0; i < asset->mesh_count; i++) {
		Mesh* mesh = &asset->meshes[i];
		free((void*)mesh->name);
		for (size_t p = 0; p < mesh->primitive_count; p++) {
			Primitive* prim = &mesh->primitives[p];
			if (prim->positions) free(prim->positions);
			if (prim->normals) free(prim->normals);
			if (prim->texcoords) free(prim->texcoords);
			if (prim->indices) free(prim->indices);
		}
		if (mesh->primitives) free(mesh->primitives);
	}
	if (asset->meshes) free(asset->meshes);

	for (size_t i = 0; i < asset->material_count; i++) free((void*)asset->materials[i].name);
	if (asset->materials) free(asset->materials);

	for (size_t i = 0; i < asset->texture_count; i++) free((void*)asset->textures[i].name);
	if (asset->textures) free(asset->textures);

	for (size_t i = 0; i < asset->image_count; i++) {
		free((void*)asset->images[i].name);
		free((void*)asset->images[i].uri);
	}
	if (asset->images) free(asset->images);

	for (size_t i = 0; i < asset->camera_count; i++) free((void*)asset->cameras[i].name);
	if (asset->cameras) free(asset->cameras);

	for (size_t i = 0; i < asset->light_count; i++) free((void*)asset->lights[i].name);
	if (asset->lights) free(asset->lights);

	for (size_t i = 0; i < asset->node_count; i++) {
		Node* node = &asset->nodes[i];
		free((void*)node->name);
		if (node->children_indices) free(node->children_indices);
		if (node->extras_json) free(node->extras_json);
	}
	if (asset->nodes) free(asset->nodes);

	free(asset);
}
