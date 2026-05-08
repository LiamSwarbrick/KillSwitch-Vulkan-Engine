#include "clustered_shading.h"
#include "internal_state.h"
#include "renderpasses/metadata.h"

#include <glm/glm.hpp>
#include "renderer/shadersrc/common/shared_buffers.glsl"

static inline b32 compute_light_cluster_bounds(
    glm::vec3 light_view_pos,
    float     light_radius,
    float     near,
    float     far,
    glm::mat4 proj,
    float     inv_log_far_over_near,

    int* tile_min_x,  int* tile_max_x,
    int* tile_min_y,  int* tile_max_y,
    int* z_index_min, int* z_index_max)
{
    // *tile_min_x   = 0;
    // *tile_min_y   = 0;
    // *z_index_min  = 0;
    // *tile_max_x   = CLUSTER_GRID_SIZE_X - 1;
    // *tile_max_y   = CLUSTER_GRID_SIZE_Y - 1;
    // *z_index_max  = CLUSTER_GRID_SIZE_Z - 1;
    // return 1;

    float r = light_radius;
    float z_center = light_view_pos.z;  // NOTE: Right-handed, so Z is negative

    // Skip lights fully behind camera (early exit)
    // NOTE: Using right handed view matrix, so z-positive is backwards (and near plane is at -near)
    if (z_center - r > -near) return 0; 
    if (z_center + r < -far)  return 0;
    

    float dist_sq = glm::dot(light_view_pos, light_view_pos);
    if (dist_sq <= (r * r)) 
    {
        // Camera is inside! The light covers the whole screen.
        *tile_min_x = 0;
        *tile_max_x = CLUSTER_GRID_SIZE_X - 1;
        *tile_min_y = 0;
        *tile_max_y = CLUSTER_GRID_SIZE_Y - 1;
    }
    else 
    {
        // AABB Projection to screen
        // NOTE: Handle the near plane properly, you can't project things behind the near plane
        //       So get the right/left/top/bottom/front/back points of the light's sphere
        //       which we'll then clamp the  of the light to the near plane
        glm::vec3 points[6] = {
            light_view_pos + glm::vec3( r,  0,  0),
            light_view_pos + glm::vec3(-r,  0,  0),
            light_view_pos + glm::vec3( 0,  r,  0),
            light_view_pos + glm::vec3( 0, -r,  0),
            light_view_pos + glm::vec3( 0,  0,  r),
            light_view_pos + glm::vec3( 0,  0, -r)
        };
        
        glm::vec2 min_ndc = glm::vec2( 1e10f);
        glm::vec2 max_ndc = glm::vec2(-1e10f);

        for (int i = 0; i < 6; i++)
        {
            glm::vec4 clip_pos = proj * glm::vec4(points[i], 1.0f);
            
            // If a point is behind the near plane, we can't just project it.
            // Solve this with clamping

            // Avoid division by zero or negative W
            float w = glm::max(clip_pos.w, 0.000001f);
            glm::vec2 ndc = glm::vec2(clip_pos.x, clip_pos.y) / w;

            min_ndc = glm::min(min_ndc, ndc);
            max_ndc = glm::max(max_ndc, ndc);
        }

        // Map NDC [-1, 1] to UV [0, 1] and clamp
        float uv_min_x = SDL_clamp(min_ndc.x * 0.5f + 0.5f, 0.0f, 0.9999f);
        float uv_max_x = SDL_clamp(max_ndc.x * 0.5f + 0.5f, 0.0f, 0.9999f);
        float uv_min_y = SDL_clamp(min_ndc.y * 0.5f + 0.5f, 0.0f, 0.9999f);
        float uv_max_y = SDL_clamp(max_ndc.y * 0.5f + 0.5f, 0.0f, 0.9999f);

        *tile_min_x = (int)(uv_min_x * CLUSTER_GRID_SIZE_X);
        *tile_max_x = (int)(uv_max_x * CLUSTER_GRID_SIZE_X);
        *tile_min_y = (int)(uv_min_y * CLUSTER_GRID_SIZE_Y);
        *tile_max_y = (int)(uv_max_y * CLUSTER_GRID_SIZE_Y);
    }

    // Z-bounds
    // NOTE: View space Z is negative, depth is positive.
    float min_depth = SDL_max(-(z_center + r), near);
    float max_depth = SDL_min(-(z_center - r), far);

    // Standard logarithmic binning formula from original paper (keeps clusters cubicly shaped)
    *z_index_min = (int)floorf(logf(min_depth / near) * inv_log_far_over_near * (float)CLUSTER_GRID_SIZE_Z);
    *z_index_max = (int)floorf(logf(max_depth / near) * inv_log_far_over_near * (float)CLUSTER_GRID_SIZE_Z);

    // Final clamps to prevent indexing out of cluster bounds
    *tile_min_x = SDL_clamp(*tile_min_x, 0, CLUSTER_GRID_SIZE_X - 1);
    *tile_max_x = SDL_clamp(*tile_max_x, 0, CLUSTER_GRID_SIZE_X - 1);
    *tile_min_y = SDL_clamp(*tile_min_y, 0, CLUSTER_GRID_SIZE_Y - 1);
    *tile_max_y = SDL_clamp(*tile_max_y, 0, CLUSTER_GRID_SIZE_Y - 1);
    *z_index_min = SDL_clamp(*z_index_min, 0, CLUSTER_GRID_SIZE_Z - 1);
    *z_index_max = SDL_clamp(*z_index_max, 0, CLUSTER_GRID_SIZE_Z - 1);

    // SDL_assert(!(
    //     *tile_min_x >  *tile_max_x ||
    //     *tile_min_y >  *tile_max_y ||
    //     *z_index_min > *z_index_max)
    // );

    return 1;
}


void ClusteredShading_CPULightAssignmentToMappedBuffer()
{
    // TODO: To test the aspect ratio is correct, use cluster grid of like 16x9xwhatever
    //       and it should be square shaped on a 16:9 monitor

    // Reset cluster staging memory
    // memset(renderstate.renderables_arena.staging_point_light_indices, 0, sizeof(uint32_t) * MAX_LIGHTS_PER_CLUSTER * CLUSTER_COUNT);
    // memset(renderstate.renderables_arena.staging_spot_light_indices, 0,  sizeof(uint32_t) * MAX_LIGHTS_PER_CLUSTER * CLUSTER_COUNT);
    for (uint32_t c = 0; c < CLUSTER_COUNT; ++c)
    {
        renderstate.renderables_arena.staging_cluster_offsets[c].point_count = 0;
        renderstate.renderables_arena.staging_cluster_offsets[c].point_offset = 0;
        renderstate.renderables_arena.staging_cluster_offsets[c].spot_count = 0;
        renderstate.renderables_arena.staging_cluster_offsets[c].spot_offset = 0;
    }


    glm::vec3 cam_pos  = renderstate.main_camera.position;
    glm::mat4 cam_view = renderstate.main_camera.view;
    
    // NOTE: Assuming swapchain's aspect ratio for the projection matrix here:
    float near   = renderstate.main_camera.near_plane;
    float far    = renderstate.main_camera.far_plane;
    glm::mat4 cam_proj = renderstate.main_camera_fullscreen_proj;
    // printf("cam_proj:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
    //     cam_proj[0][0], cam_proj[1][0], cam_proj[2][0], cam_proj[3][0],
    //     cam_proj[0][1], cam_proj[1][1], cam_proj[2][1], cam_proj[3][1],
    //     cam_proj[0][2], cam_proj[1][2], cam_proj[2][2], cam_proj[3][2],
    //     cam_proj[0][3], cam_proj[1][3], cam_proj[2][3], cam_proj[3][3]
    // );
    float inv_log_far_over_near = 1.0f / logf(far / near);

    for (uint32_t i = 0; i < renderstate.renderables_arena.num_point_lights; ++i)
    {
        PointLight pl = renderstate.renderables_arena.point_lights[i];
        glm::vec3 light_view_pos = glm::vec3(cam_view * glm::vec4(pl.pos_and_radius[0], pl.pos_and_radius[1], pl.pos_and_radius[2], 1.0f));
        float light_radius       = pl.pos_and_radius[3];
        
        int tile_min_x,  tile_max_x;
        int tile_min_y,  tile_max_y;
        int z_index_min, z_index_max;

        if (!compute_light_cluster_bounds(light_view_pos, light_radius, near, far, cam_proj, inv_log_far_over_near,
            &tile_min_x, &tile_max_x, &tile_min_y, &tile_max_y, &z_index_min, &z_index_max))
        {
            continue;
        }
        
        // Add to each cluster the light affects
        for (int z = z_index_min; z <= z_index_max; ++z)
        for (int y = tile_min_y;  y <= tile_max_y;  ++y)
        for (int x = tile_min_x;  x <= tile_max_x;  ++x)
        {
            uint32_t cluster_index = CLUSTER_INDEX(x, y, z);
            Cluster* c = &renderstate.renderables_arena.staging_cluster_offsets[cluster_index];

            // Add light to cluster
            if (c->point_count < MAX_LIGHTS_PER_CLUSTER)
            {
                uint32_t local_idx = c->point_count++;
                renderstate.renderables_arena.staging_point_light_indices[cluster_index * MAX_LIGHTS_PER_CLUSTER + local_idx] = i;
            }
        }
    }

    for (uint32_t i = 0; i < renderstate.renderables_arena.num_spot_lights; ++i)
    {
        SpotLight sl = renderstate.renderables_arena.spot_lights[i];
        glm::vec3 light_view_pos = glm::vec3(cam_view * glm::vec4(sl.pos_and_radius[0], sl.pos_and_radius[1], sl.pos_and_radius[2], 1.0f));
        float light_radius       = sl.pos_and_radius[3];
        
        int tile_min_x,  tile_max_x;
        int tile_min_y,  tile_max_y;
        int z_index_min, z_index_max;

        if (!compute_light_cluster_bounds(light_view_pos, light_radius, near, far, cam_proj, inv_log_far_over_near,
            &tile_min_x, &tile_max_x, &tile_min_y, &tile_max_y, &z_index_min, &z_index_max))
        {
            continue;
        }

        // TODO: Cone based culling too.
        
        // Add to each cluster the light affects
        for (int z = z_index_min; z <= z_index_max; ++z)
        for (int y = tile_min_y;  y <= tile_max_y;  ++y)
        for (int x = tile_min_x;  x <= tile_max_x;  ++x)
        {
            uint32_t cluster_index = CLUSTER_INDEX(x, y, z);
            Cluster* c = &renderstate.renderables_arena.staging_cluster_offsets[cluster_index];

            // Add light to cluster
            if (c->spot_count < MAX_LIGHTS_PER_CLUSTER)
            {
                uint32_t local_idx = c->spot_count++;
                renderstate.renderables_arena.staging_spot_light_indices[cluster_index * MAX_LIGHTS_PER_CLUSTER + local_idx] = i;
            }
        }
    }
    
    // Copy to GPU....
    RingBufferedRIDs* ring = &renderstate.rids.ring[renderstate.frame_in_flight];
    FG_Resource* point_light_indices_buffer_res = &renderstate.registry.resources[ring->point_light_indices_buffer_rid];
    FG_Resource* spot_light_indices_buffer_res = &renderstate.registry.resources[ring->spot_light_indices_buffer_rid];
    FG_Resource* cluster_offsets_buffer_res = &renderstate.registry.resources[ring->cluster_offsets_buffer_rid];


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
            &renderstate.renderables_arena.staging_point_light_indices[c * MAX_LIGHTS_PER_CLUSTER],
            cluster.point_count * sizeof(uint32_t)
        );
        memcpy(
            &mapped_spot_light_indices[cluster.spot_offset],
            &renderstate.renderables_arena.staging_spot_light_indices[c * MAX_LIGHTS_PER_CLUSTER],
            cluster.spot_count * sizeof(uint32_t)
        );

        // printf("Cluster %d: point offset %u, plcount %u, spotoffset %u, spot count %u\n", c, cluster.point_offset, cluster.point_count, cluster.spot_offset, cluster.spot_count);

        // Copy cluster metadata
        mapped_clusters[c] = cluster;
    }
    Cluster last_cluster = renderstate.renderables_arena.staging_cluster_offsets[CLUSTER_COUNT-1];
    vmaFlushAllocation(renderstate.vma_allocator, point_light_indices_buffer_res->allocation, 0, sizeof(uint32_t) * (last_cluster.point_offset + last_cluster.point_count));
    vmaFlushAllocation(renderstate.vma_allocator, spot_light_indices_buffer_res->allocation, 0, sizeof(uint32_t) * (last_cluster.spot_offset + last_cluster.spot_count));
    vmaFlushAllocation(renderstate.vma_allocator, cluster_offsets_buffer_res->allocation, 0, sizeof(Cluster) * CLUSTER_COUNT);
}
