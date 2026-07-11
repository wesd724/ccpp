#ifndef FLUID_SIM_H
#define FLUID_SIM_H

#include "math3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLUID_RESOLUTION 256
#define FLUID_GRID_WIDTH (FLUID_RESOLUTION + 1)
#define FLUID_VERTEX_COUNT (FLUID_GRID_WIDTH * FLUID_GRID_WIDTH)
#define FLUID_CELL_COUNT (FLUID_RESOLUTION * FLUID_RESOLUTION)
#define FLUID_WORLD_SIZE 48.0f
#define FLUID_FIXED_STEP (1.0f / 120.0f)
#define FLUID_MAX_SPRAY 7000

typedef struct {
    float position[3];
    float normal[3];
    float foam;
    float velocity;
} FluidVertex;

typedef struct {
    Vec3 position;
    Vec3 velocity;
    float life;
    float max_life;
    float size;
} SprayParticle;

typedef struct {
    float *height;
    float *next_height;
    float *velocity_x;
    float *velocity_z;
    float *next_velocity_x;
    float *next_velocity_z;
    float *foam;
    FluidVertex *vertices;
    SprayParticle spray[FLUID_MAX_SPRAY];
    size_t spray_count;
    uint32_t random_state;
    int wave_cycle;
} FluidSim;

bool fluid_sim_init(FluidSim *sim);
void fluid_sim_destroy(FluidSim *sim);
void fluid_sim_reset(FluidSim *sim);
void fluid_sim_step(FluidSim *sim, float delta_time, float time_seconds);
void fluid_sim_disturb(FluidSim *sim, float world_x, float world_z, float strength, float radius);
void fluid_sim_launch_collision(FluidSim *sim, float strength);
bool fluid_sim_sample(const FluidSim *sim, float world_x, float world_z,
                      float *height, float *velocity_x, float *velocity_z);
void fluid_sim_update_vertices(FluidSim *sim, float interpolation);
const FluidVertex *fluid_sim_vertices(const FluidSim *sim);
const SprayParticle *fluid_sim_spray(const FluidSim *sim, size_t *count);

#endif
