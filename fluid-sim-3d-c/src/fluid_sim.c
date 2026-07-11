#include "fluid_sim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GRID_STEP (FLUID_WORLD_SIZE / (float)FLUID_RESOLUTION)
#define BASE_DEPTH 2.35f

static int grid_id(int x, int z) {
    return z * FLUID_GRID_WIDTH + x;
}

static float world_coordinate(int index) {
    return ((float)index / (float)FLUID_RESOLUTION - 0.5f) * FLUID_WORLD_SIZE;
}

static float smoothstepf(float low, float high, float value) {
    float t = clampf((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t next_random(FluidSim *sim) {
    sim->random_state = sim->random_state * 1664525u + 1013904223u;
    return sim->random_state;
}

static float random_unit(FluidSim *sim) {
    return (float)(next_random(sim) >> 8) * (1.0f / 16777216.0f);
}

static float sample_field(const float *field, float x, float z) {
    x = clampf(x, 1.0f, (float)FLUID_RESOLUTION - 1.001f);
    z = clampf(z, 1.0f, (float)FLUID_RESOLUTION - 1.001f);
    int x0 = (int)x;
    int z0 = (int)z;
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    float tx = x - (float)x0;
    float tz = z - (float)z0;
    float a = field[grid_id(x0, z0)] * (1.0f - tx) + field[grid_id(x1, z0)] * tx;
    float b = field[grid_id(x0, z1)] * (1.0f - tx) + field[grid_id(x1, z1)] * tx;
    return a * (1.0f - tz) + b * tz;
}

static float interpolated_height_at(const FluidSim *sim, int x, int z, float interpolation) {
    x = (int)clampf((float)x, 0.0f, (float)FLUID_RESOLUTION);
    z = (int)clampf((float)z, 0.0f, (float)FLUID_RESOLUTION);
    int id = grid_id(x, z);
    return sim->next_height[id] * (1.0f - interpolation) + sim->height[id] * interpolation;
}

static void spawn_spray(FluidSim *sim, int x, int z, float energy, float slope_x, float slope_z) {
    if (sim->spray_count >= FLUID_MAX_SPRAY) return;
    SprayParticle *particle = &sim->spray[sim->spray_count++];
    int id = grid_id(x, z);
    float jitter_x = (random_unit(sim) - 0.5f) * GRID_STEP * 2.0f;
    float jitter_z = (random_unit(sim) - 0.5f) * GRID_STEP * 2.0f;
    particle->position = vec3(world_coordinate(x) + jitter_x, sim->height[id] + 0.08f,
                              world_coordinate(z) + jitter_z);
    particle->velocity = vec3(sim->velocity_x[id] - slope_x * energy * 0.8f + (random_unit(sim) - 0.5f),
                              1.5f + energy * (1.2f + random_unit(sim) * 1.8f),
                              sim->velocity_z[id] - slope_z * energy * 0.8f + (random_unit(sim) - 0.5f));
    particle->life = 0.55f + random_unit(sim) * 1.05f;
    particle->max_life = particle->life;
    particle->size = 2.5f + random_unit(sim) * 5.5f;
}

static void update_spray(FluidSim *sim, float delta_time) {
    size_t i = 0;
    while (i < sim->spray_count) {
        SprayParticle *particle = &sim->spray[i];
        particle->velocity.y -= 9.81f * delta_time;
        particle->velocity = vec3_scale(particle->velocity, expf(-0.16f * delta_time));
        particle->position = vec3_add(particle->position, vec3_scale(particle->velocity, delta_time));
        particle->life -= delta_time;
        if (particle->life <= 0.0f || particle->position.y < -0.15f) {
            sim->spray[i] = sim->spray[--sim->spray_count];
        } else {
            ++i;
        }
    }
}

bool fluid_sim_init(FluidSim *sim) {
    memset(sim, 0, sizeof(*sim));
    sim->height = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->next_height = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->velocity_x = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->velocity_z = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->next_velocity_x = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->next_velocity_z = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->foam = calloc(FLUID_VERTEX_COUNT, sizeof(float));
    sim->vertices = calloc(FLUID_VERTEX_COUNT, sizeof(FluidVertex));
    if (!sim->height || !sim->next_height || !sim->velocity_x || !sim->velocity_z ||
        !sim->next_velocity_x || !sim->next_velocity_z || !sim->foam || !sim->vertices) {
        fluid_sim_destroy(sim);
        return false;
    }
    sim->random_state = 0x9e3779b9u;
    fluid_sim_reset(sim);
    return true;
}

void fluid_sim_destroy(FluidSim *sim) {
    free(sim->height);
    free(sim->next_height);
    free(sim->velocity_x);
    free(sim->velocity_z);
    free(sim->next_velocity_x);
    free(sim->next_velocity_z);
    free(sim->foam);
    free(sim->vertices);
    memset(sim, 0, sizeof(*sim));
}

void fluid_sim_disturb(FluidSim *sim, float world_x, float world_z, float strength, float radius) {
    float radius_squared = radius * radius;
    int center_x = (int)((world_x / FLUID_WORLD_SIZE + 0.5f) * FLUID_RESOLUTION);
    int center_z = (int)((world_z / FLUID_WORLD_SIZE + 0.5f) * FLUID_RESOLUTION);
    int grid_radius = (int)ceilf(radius / GRID_STEP) + 1;
    int min_x = (int)clampf((float)(center_x - grid_radius), 1.0f, FLUID_RESOLUTION - 1.0f);
    int max_x = (int)clampf((float)(center_x + grid_radius), 1.0f, FLUID_RESOLUTION - 1.0f);
    int min_z = (int)clampf((float)(center_z - grid_radius), 1.0f, FLUID_RESOLUTION - 1.0f);
    int max_z = (int)clampf((float)(center_z + grid_radius), 1.0f, FLUID_RESOLUTION - 1.0f);
    for (int z = min_z; z <= max_z; ++z) {
        float dz = world_coordinate(z) - world_z;
        if (fabsf(dz) > radius) continue;
        for (int x = min_x; x <= max_x; ++x) {
            float dx = world_coordinate(x) - world_x;
            float distance_squared = dx * dx + dz * dz;
            if (distance_squared >= radius_squared) continue;
            int id = grid_id(x, z);
            float weight = expf(-distance_squared / (radius_squared * 0.28f));
            sim->height[id] += strength * weight;
            sim->foam[id] = fmaxf(sim->foam[id], fabsf(strength) * weight * 0.22f);
            float inverse_distance = 1.0f / sqrtf(distance_squared + 0.04f);
            sim->velocity_x[id] += dx * inverse_distance * strength * weight * 0.72f;
            sim->velocity_z[id] += dz * inverse_distance * strength * weight * 0.72f;
        }
    }
}

bool fluid_sim_sample(const FluidSim *sim, float world_x, float world_z,
                      float *height, float *velocity_x, float *velocity_z) {
    float half_size = FLUID_WORLD_SIZE * 0.5f;
    if (world_x < -half_size || world_x > half_size ||
        world_z < -half_size || world_z > half_size) return false;
    float grid_x = (world_x / FLUID_WORLD_SIZE + 0.5f) * FLUID_RESOLUTION;
    float grid_z = (world_z / FLUID_WORLD_SIZE + 0.5f) * FLUID_RESOLUTION;
    if (height) *height = sample_field(sim->height, grid_x, grid_z);
    if (velocity_x) *velocity_x = sample_field(sim->velocity_x, grid_x, grid_z);
    if (velocity_z) *velocity_z = sample_field(sim->velocity_z, grid_x, grid_z);
    return true;
}

void fluid_sim_launch_collision(FluidSim *sim, float strength) {
#pragma omp parallel for schedule(static)
    for (int z = 2; z < FLUID_RESOLUTION - 1; ++z) {
        float world_z = world_coordinate(z);
        float lateral = expf(-(world_z * world_z) / 210.0f);
        for (int x = 2; x < FLUID_RESOLUTION - 1; ++x) {
            float world_x = world_coordinate(x);
            float left_distance = world_x + 17.0f + world_z * 0.10f + sinf(world_z * 0.23f) * 0.75f;
            float right_distance = world_x - 17.0f + world_z * 0.075f - sinf(world_z * 0.19f) * 0.65f;
            float left_wave = expf(-(left_distance * left_distance) / 5.8f) * lateral;
            float right_wave = expf(-(right_distance * right_distance) / 5.8f) * lateral;
            int id = grid_id(x, z);
            float skew = 0.88f + 0.12f * cosf(world_z * 0.28f);
            sim->height[id] += strength * (left_wave + right_wave) * skew;
            sim->velocity_x[id] += strength * 1.45f * (left_wave - right_wave);
        }
    }
}

void fluid_sim_reset(FluidSim *sim) {
    memset(sim->height, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->next_height, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->velocity_x, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->velocity_z, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->next_velocity_x, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->next_velocity_z, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    memset(sim->foam, 0, sizeof(float) * FLUID_VERTEX_COUNT);
    sim->spray_count = 0;
    sim->wave_cycle = 0;
    fluid_sim_launch_collision(sim, 2.25f);
    fluid_sim_update_vertices(sim, 1.0f);
}

static void apply_wave_makers(FluidSim *sim, float time_seconds) {
#pragma omp parallel for schedule(static)
    for (int z = 2; z < FLUID_RESOLUTION - 1; ++z) {
        float world_z = world_coordinate(z);
        float envelope = 0.5f + 0.5f * expf(-(world_z * world_z) / 250.0f);
        float left_target = 1.15f * envelope * sinf(time_seconds * 1.45f + world_z * 0.10f);
        float right_target = 1.15f * envelope * sinf(time_seconds * 1.45f + world_z * 0.10f + 2.35f);
        for (int band = 2; band <= 7; ++band) {
            float blend = (float)(band - 1) / 7.0f;
            int left = grid_id(band, z);
            int right = grid_id(FLUID_RESOLUTION - band, z);
            sim->height[left] += (left_target - sim->height[left]) * 0.018f * blend;
            sim->height[right] += (right_target - sim->height[right]) * 0.018f * blend;
            sim->velocity_x[left] += 0.0075f * left_target * blend;
            sim->velocity_x[right] -= 0.0075f * right_target * blend;
        }
    }
}

void fluid_sim_step(FluidSim *sim, float delta_time, float time_seconds) {
    const float gravity = 9.81f;
    const float inverse_step = 1.0f / GRID_STEP;
    const float inverse_step_squared = inverse_step * inverse_step;
    const float viscosity = 0.028f;
    const float velocity_decay = expf(-0.055f * delta_time);

#pragma omp parallel
    {
#pragma omp for schedule(static)
    for (int z = 1; z < FLUID_RESOLUTION; ++z) {
        for (int x = 1; x < FLUID_RESOLUTION; ++x) {
            int id = grid_id(x, z);
            float vx = sim->velocity_x[id];
            float vz = sim->velocity_z[id];
            float back_x = (float)x - vx * delta_time * inverse_step;
            float back_z = (float)z - vz * delta_time * inverse_step;
            float advected_x = sample_field(sim->velocity_x, back_x, back_z);
            float advected_z = sample_field(sim->velocity_z, back_x, back_z);
            float gradient_x = (sim->height[grid_id(x + 1, z)] - sim->height[grid_id(x - 1, z)]) * 0.5f * inverse_step;
            float gradient_z = (sim->height[grid_id(x, z + 1)] - sim->height[grid_id(x, z - 1)]) * 0.5f * inverse_step;
            float laplacian_x = (sim->velocity_x[grid_id(x + 1, z)] + sim->velocity_x[grid_id(x - 1, z)] +
                                 sim->velocity_x[grid_id(x, z + 1)] + sim->velocity_x[grid_id(x, z - 1)] - 4.0f * vx) * inverse_step_squared;
            float laplacian_z = (sim->velocity_z[grid_id(x + 1, z)] + sim->velocity_z[grid_id(x - 1, z)] +
                                 sim->velocity_z[grid_id(x, z + 1)] + sim->velocity_z[grid_id(x, z - 1)] - 4.0f * vz) * inverse_step_squared;
            sim->next_velocity_x[id] = (advected_x - gravity * gradient_x * delta_time + viscosity * laplacian_x * delta_time) * velocity_decay;
            sim->next_velocity_z[id] = (advected_z - gravity * gradient_z * delta_time + viscosity * laplacian_z * delta_time) * velocity_decay;
        }
    }

#pragma omp for schedule(static)
    for (int z = 1; z < FLUID_RESOLUTION; ++z) {
        for (int x = 1; x < FLUID_RESOLUTION; ++x) {
            int id = grid_id(x, z);
            float height_left = fmaxf(0.18f, BASE_DEPTH + sim->height[grid_id(x - 1, z)]);
            float height_right = fmaxf(0.18f, BASE_DEPTH + sim->height[grid_id(x + 1, z)]);
            float height_down = fmaxf(0.18f, BASE_DEPTH + sim->height[grid_id(x, z - 1)]);
            float height_up = fmaxf(0.18f, BASE_DEPTH + sim->height[grid_id(x, z + 1)]);
            float flux_divergence = (height_right * sim->next_velocity_x[grid_id(x + 1, z)] -
                                     height_left * sim->next_velocity_x[grid_id(x - 1, z)] +
                                     height_up * sim->next_velocity_z[grid_id(x, z + 1)] -
                                     height_down * sim->next_velocity_z[grid_id(x, z - 1)]) * 0.5f * inverse_step;
            float next = sim->height[id] - flux_divergence * delta_time;
            sim->next_height[id] = clampf(next * expf(-0.008f * delta_time), -1.95f, 3.45f);

            float slope_x = (sim->height[grid_id(x + 1, z)] - sim->height[grid_id(x - 1, z)]) * 0.5f * inverse_step;
            float slope_z = (sim->height[grid_id(x, z + 1)] - sim->height[grid_id(x, z - 1)]) * 0.5f * inverse_step;
            float slope = sqrtf(slope_x * slope_x + slope_z * slope_z);
            float compression = fmaxf(-flux_divergence, 0.0f);
            float speed = hypotf(sim->next_velocity_x[id], sim->next_velocity_z[id]);
            float breaking = smoothstepf(0.72f, 1.62f, slope) * smoothstepf(0.52f, 2.7f, compression + speed * 0.14f);
            sim->foam[id] = clampf(fmaxf(sim->foam[id] * expf(-0.95f * delta_time), breaking), 0.0f, 1.0f);
        }
    }
    }

    for (int z = 2; z < FLUID_RESOLUTION - 1; z += 2) {
        for (int x = 2; x < FLUID_RESOLUTION - 1; x += 2) {
            int id = grid_id(x, z);
            float breaking = sim->foam[id];
            if (breaking <= 0.62f || sim->height[id] <= 0.45f ||
                random_unit(sim) >= breaking * 0.075f) continue;
            float slope_x = (sim->height[grid_id(x + 1, z)] - sim->height[grid_id(x - 1, z)]) * 0.5f * inverse_step;
            float slope_z = (sim->height[grid_id(x, z + 1)] - sim->height[grid_id(x, z - 1)]) * 0.5f * inverse_step;
            float speed = hypotf(sim->next_velocity_x[id], sim->next_velocity_z[id]);
            spawn_spray(sim, x, z, breaking * (1.0f + speed * 0.22f), slope_x, slope_z);
        }
    }

    for (int i = 0; i <= FLUID_RESOLUTION; ++i) {
        sim->next_height[grid_id(0, i)] = sim->next_height[grid_id(1, i)];
        sim->next_height[grid_id(FLUID_RESOLUTION, i)] = sim->next_height[grid_id(FLUID_RESOLUTION - 1, i)];
        sim->next_height[grid_id(i, 0)] = sim->next_height[grid_id(i, 1)];
        sim->next_height[grid_id(i, FLUID_RESOLUTION)] = sim->next_height[grid_id(i, FLUID_RESOLUTION - 1)];
        sim->next_velocity_x[grid_id(0, i)] = 0.0f;
        sim->next_velocity_x[grid_id(FLUID_RESOLUTION, i)] = 0.0f;
        sim->next_velocity_z[grid_id(i, 0)] = 0.0f;
        sim->next_velocity_z[grid_id(i, FLUID_RESOLUTION)] = 0.0f;
    }

    float *temporary = sim->height;
    sim->height = sim->next_height;
    sim->next_height = temporary;
    temporary = sim->velocity_x;
    sim->velocity_x = sim->next_velocity_x;
    sim->next_velocity_x = temporary;
    temporary = sim->velocity_z;
    sim->velocity_z = sim->next_velocity_z;
    sim->next_velocity_z = temporary;

    apply_wave_makers(sim, time_seconds);
    int cycle = (int)(time_seconds / 7.5f);
    if (cycle > sim->wave_cycle) {
        sim->wave_cycle = cycle;
        fluid_sim_launch_collision(sim, 1.85f);
    }
    update_spray(sim, delta_time);
}

void fluid_sim_update_vertices(FluidSim *sim, float interpolation) {
    interpolation = clampf(interpolation, 0.0f, 1.0f);
    float inverse_double_step = 1.0f / (2.0f * GRID_STEP);
#pragma omp parallel for schedule(static)
    for (int z = 0; z <= FLUID_RESOLUTION; ++z) {
        for (int x = 0; x <= FLUID_RESOLUTION; ++x) {
            int id = grid_id(x, z);
            float height = interpolated_height_at(sim, x, z, interpolation);
            float slope_x = (interpolated_height_at(sim, x + 1, z, interpolation) -
                             interpolated_height_at(sim, x - 1, z, interpolation)) * inverse_double_step;
            float slope_z = (interpolated_height_at(sim, x, z + 1, interpolation) -
                             interpolated_height_at(sim, x, z - 1, interpolation)) * inverse_double_step;
            Vec3 normal = vec3_normalize(vec3(-slope_x, 1.0f, -slope_z));
            FluidVertex *vertex = &sim->vertices[id];
            vertex->position[0] = world_coordinate(x);
            vertex->position[1] = height;
            vertex->position[2] = world_coordinate(z);
            vertex->normal[0] = normal.x;
            vertex->normal[1] = normal.y;
            vertex->normal[2] = normal.z;
            vertex->foam = sim->foam[id];
            float velocity_x = sim->next_velocity_x[id] * (1.0f - interpolation) +
                               sim->velocity_x[id] * interpolation;
            float velocity_z = sim->next_velocity_z[id] * (1.0f - interpolation) +
                               sim->velocity_z[id] * interpolation;
            vertex->velocity = hypotf(velocity_x, velocity_z);
        }
    }
}

const FluidVertex *fluid_sim_vertices(const FluidSim *sim) {
    return sim->vertices;
}

const SprayParticle *fluid_sim_spray(const FluidSim *sim, size_t *count) {
    *count = sim->spray_count;
    return sim->spray;
}
