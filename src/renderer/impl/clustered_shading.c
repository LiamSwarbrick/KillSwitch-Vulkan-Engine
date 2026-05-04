#include "clustered_shading.h"
#include "internal_state.h"
#include "renderpasses/metadata.h"

static inline b32 compute_light_cluster_bounds(
    // In
    glm::vec3 light_view_pos,
    float     light_radius,
    float     near,
    float     far,
    glm::mat4 proj,
    float     log_far_over_near,

    // Out
    int* tile_min_x,  int* tile_max_x,
    int* tile_min_y,  int* tile_max_y,
    int* z_index_min, int* z_index_max)
{
    float x = light_view_pos.x;
    float y = light_view_pos.y;
    float z = light_view_pos.z;
    float r = light_radius;

    float depth = -z;
    float min_depth = depth - r;
    float max_depth = depth + r;

    // Skip lights fully behind camera (early exit)
    // NOTE: Using right handed view matrix, so z-positive is backwards (and near plane is at -near)
    if (max_depth <= near) return 0;
    if (min_depth >= far)  return 0;  // <- Also checking if it's past the far plane

    float inv_z = 1.0f / z;
    float min_x = (x - r) * inv_z;
    float max_x = (x + r) * inv_z;
    float min_y = (y - r) * inv_z;
    float max_y = (y + r) * inv_z;

    // Project to screen (NOTE: Assuming just using this part of the proj matrix enough)
    float proj_x = proj[0][0];
    float proj_y = proj[1][1];

    min_x *= proj_x;
    max_x *= proj_x;
    min_y *= proj_y;
    max_y *= proj_y;

    // Min/Max tile ids that fill this cluster's screenspace AABB
    *tile_min_x = (int)((min_x * 0.5f + 0.5f) * CLUSTER_GRID_SIZE_X);
    *tile_max_x = (int)((max_x * 0.5f + 0.5f) * CLUSTER_GRID_SIZE_X);
    *tile_min_y = (int)((min_y * 0.5f + 0.5f) * CLUSTER_GRID_SIZE_Y);
    *tile_max_y = (int)((max_y * 0.5f + 0.5f) * CLUSTER_GRID_SIZE_Y);

    // Min/Max Z slice index
    min_depth = SDL_max(min_depth, near);
    max_depth = SDL_min(max_depth, far);
    float log_min = log(min_depth / near) / log_far_over_near;
    float log_max = log(max_depth / near) / log_far_over_near;

    *z_index_min = (int)(log_min * CLUSTER_GRID_SIZE_Z);
    *z_index_max = (int)(log_max * CLUSTER_GRID_SIZE_Z);

    // Clamp so we don't invalidly index our array
    *tile_min_x  = SDL_clamp(*tile_min_x,  0, CLUSTER_GRID_SIZE_X - 1);
    *tile_max_x  = SDL_clamp(*tile_max_x,  0, CLUSTER_GRID_SIZE_X - 1);
    *tile_min_y  = SDL_clamp(*tile_min_y,  0, CLUSTER_GRID_SIZE_Y - 1);
    *tile_max_y  = SDL_clamp(*tile_max_y,  0, CLUSTER_GRID_SIZE_Y - 1);
    *z_index_min = SDL_clamp(*z_index_min, 0, CLUSTER_GRID_SIZE_Z - 1);
    *z_index_max = SDL_clamp(*z_index_max, 0, CLUSTER_GRID_SIZE_Z - 1);

    return 1;
}

void ClusteredShading_CPULightAssignmentToMappedBuffer()
{
    // TODO: To test the aspect ratio is correct, use cluster grid of like 16x9xwhatever
    //       and it should be square shaped on a 16:9 monitor

    // Reset cluster staging arenas (no need to reset staging_light_indices arena though)
    memset(renderstate.renderables_arena.staging_cluster_offsets, 0, sizeof(Cluster) * CLUSTER_COUNT);


    glm::vec3 cam_pos  = renderstate.main_camera.position;
    glm::mat4 cam_view = renderstate.main_camera.view;
    
    // NOTE: Assuming swapchain's aspect ratio for the projection matrix here:
    float aspect = (float)renderstate.swapchain_extent.width / (float)renderstate.swapchain_extent.height;
    float near   = renderstate.main_camera.near;
    float far    = renderstate.main_camera.far;
    glm::mat4 cam_proj = MakeProjectionMatrix(renderstate.settings.fov_y, aspect, near, far);
    float log_far_over_near = log(far / near);

    for (uint32_t i = 0; i < renderstate.renderables_arena.num_point_lights; ++i)
    {
        PointLight pl = renderstate.renderables_arena.point_lights[i];
        glm::vec3 light_view_pos = glm::vec3(cam_view * glm::vec4(pl.pos_and_radius.x, pl.pos_and_radius.y, pl.pos_and_radius.z, 1.0f));
        float light_radius       = pl.pos_and_radius.w;
        
        int tile_min_x,  tile_max_x;
        int tile_min_y,  tile_max_y;
        int z_index_min, z_index_max;

        if (!compute_light_cluster_bounds(light_view_pos, light_radius, near, far, cam_proj, log_far_over_near,
            &tile_min_x, &tile_max_x, &tile_min_y, &tile_max_y, &z_index_min, &z_index_max))
        {
            continue;
        }
        
        // Add to each cluster the light affects
        for (uint32_t z = z_index_min; z <= z_index_max; ++z)
        for (uint32_t y = tile_min_y;  y <= tile_max_y;  ++y)
        for (uint32_t x = tile_min_x;  x <= tile_max_x;  ++x)
        {
            uint32_t cluster_index = CLUSTER_INDEX(x, y, z);

            // Add light to cluster
            uint32_t light_indices_index = renderstate.renderables_arena.staging_cluster_offsets[cluster_index].point_count++;
            renderstate.renderables_arena.staging_point_light_indices[cluster_index * MAX_POINTLIGHTS + light_indices_index] = i;
        }
    }

    for (uint32_t i = 0; i < renderstate.renderables_arena.num_spot_lights; ++i)
    {
        SpotLight sl = renderstate.renderables_arena.spot_lights[i];
        glm::vec3 light_view_pos = glm::vec3(cam_view * glm::vec4(sl.pos_and_radius.x, sl.pos_and_radius.y, sl.pos_and_radius.z, 1.0f));
        float light_radius       = sl.pos_and_radius.w;
        
        int tile_min_x,  tile_max_x;
        int tile_min_y,  tile_max_y;
        int z_index_min, z_index_max;

        if (!compute_light_cluster_bounds(light_view_pos, light_radius, near, far, cam_proj, log_far_over_near,
            &tile_min_x, &tile_max_x, &tile_min_y, &tile_max_y, &z_index_min, &z_index_max))
        {
            continue;
        }

        // TODO: Cone based culling too.
        
        // Add to each cluster the light affects
        for (uint32_t z = z_index_min; z <= z_index_max; ++z)
        for (uint32_t y = tile_min_y;  y <= tile_max_y;  ++y)
        for (uint32_t x = tile_min_x;  x <= tile_max_x;  ++x)
        {
            uint32_t cluster_index = CLUSTER_INDEX(x, y, z);

            // Add light to cluster
            uint32_t light_indices_index = renderstate.renderables_arena.staging_cluster_offsets[cluster_index].spot_count++;
            renderstate.renderables_arena.staging_spot_light_indices[cluster_index * MAX_SPOTLIGHTS + light_indices_index] = i;
        }

    }

    // Copy to GPU....
    FG_Resource* point_light_indices_buffer_res = &renderstate.registry.resources[renderstate.rids.point_light_indices_buffer_rid];
    FG_Resource* spot_light_indices_buffer_res = &renderstate.registry.resources[renderstate.rids.spot_light_indices_buffer_rid];
    FG_Resource* cluster_offsets_buffer_res = &renderstate.registry.resources[renderstate.rids.cluster_offsets_buffer_rid];

    uint32_t* mapped_point_light_indices = (uint32_t*)point_light_indices_buffer_res->buffer.mapped_data;
    uint32_t* mapped_spot_light_indices  = (uint32_t*)spot_light_indices_buffer_res->buffer.mapped_data;
    Cluster* mapped_clusters = (Cluster*)cluster_offsets_buffer_res->buffer.mapped_data;

    // Compute offsets for which slices of light indices belong to each cluster
    renderstate.renderables_arena.staging_cluster_offsets[0].point_offset = 0;
    renderstate.renderables_arena.staging_cluster_offsets[0].spot_offset = 0;
    for (uint32_t c = 0; c < CLUSTER_COUNT; ++c)
    {
        if (c > 0)
        {
            renderstate.renderables_arena.staging_cluster_offsets[c].point_offset =
                renderstate.renderables_arena.staging_cluster_offsets[c-1].point_offset + renderstate.renderables_arena.staging_cluster_offsets[c-1].point_count;

            renderstate.renderables_arena.staging_cluster_offsets[c].spot_offset =
                renderstate.renderables_arena.staging_cluster_offsets[c-1].spot_offset + renderstate.renderables_arena.staging_cluster_offsets[c-1].spot_count;
        }
        else
        {
            renderstate.renderables_arena.staging_cluster_offsets[c].point_offset = 0;
            renderstate.renderables_arena.staging_cluster_offsets[c].spot_offset  = 0;
        }

        // Copy slices to packed mapped buffer
        Cluster cluster = renderstate.renderables_arena.staging_cluster_offsets[c];
        memcpy(
            &mapped_point_light_indices[cluster.point_offset],
            &renderstate.renderables_arena.staging_point_light_indices[c * MAX_POINTLIGHTS],
            cluster.point_count * sizeof(uint32_t)
        );
        memcpy(
            &mapped_point_light_indices[cluster.spot_offset],
            &renderstate.renderables_arena.staging_spot_light_indices[c * MAX_SPOTLIGHTS],
            cluster.spot_count * sizeof(uint32_t)
        );

        // Copy cluster metadata
        mapped_clusters[c] = cluster;
    }
    Cluster last_cluster = renderstate.renderables_arena.staging_cluster_offsets[CLUSTER_COUNT-1];
    vmaFlushAllocation(renderstate.vma_allocator, point_light_indices_buffer_res->allocation, 0, sizeof(uint32_t) * (last_cluster.point_offset + last_cluster.point_count));
    vmaFlushAllocation(renderstate.vma_allocator, spot_light_indices_buffer_res->allocation, 0, sizeof(uint32_t) * (last_cluster.spot_offset + last_cluster.spot_count));
    vmaFlushAllocation(renderstate.vma_allocator, cluster_offsets_buffer_res->allocation, 0, sizeof(Cluster) * CLUSTER_COUNT);
}
