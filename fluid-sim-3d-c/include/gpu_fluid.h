#ifndef GPU_FLUID_H
#define GPU_FLUID_H

#include "camera.h"
#include "gl_api.h"
#include "gpu_surface.h"

#include <stdbool.h>
#include <stddef.h>

#define GPU_FLUID_MAX_IMPULSES 4
#define GPU_FLUID_DOMAIN_HALF_EXTENT 8.0f
#define GPU_FLUID_INTERACTION_SURFACE_Y (-0.45f)

typedef enum {
    GPU_FLUID_REALTIME,
    GPU_FLUID_HIGH_QUALITY,
} GpuFluidQuality;

typedef struct {
    float position[4];
    float velocity[4];
} GpuFluidParticle;

typedef struct {
    Vec3 center;
    Vec3 radius;
    Vec3 acceleration;
} GpuFluidImpulse;

typedef struct {
    GLuint particle_buffers[2];
    GLuint particle_vaos[2];
    GLuint grid_accum;
    GLuint grid_velocity;
    GLuint grid_velocity_old;
    GLuint divergence;
    GLuint pressure[2];
    GLuint clear_program;
    GLuint p2g_program;
    GLuint normalize_program;
    GLuint force_program;
    GLuint divergence_program;
    GLuint pressure_program;
    GLuint project_program;
    GLuint g2p_program;
    GLuint render_program;
    GpuSurface surface;
    GpuFluidImpulse impulses[GPU_FLUID_MAX_IMPULSES];
    size_t particle_count;
    size_t max_particles;
    size_t grid_cells;
    int grid_x;
    int grid_y;
    int grid_z;
    int pressure_iterations;
    int current_particles;
    int impulse_count;
    float dx;
    float particle_spacing;
    float particle_radius;
    float maximum_step;
    GpuFluidQuality quality;
    bool initialized;
} GpuFluid;

bool gpu_fluid_init(GpuFluid *fluid, GpuFluidQuality quality);
void gpu_fluid_reset(GpuFluid *fluid);
void gpu_fluid_disturb(GpuFluid *fluid, Vec3 position, Vec3 drag_velocity);
void gpu_fluid_launch_collision(GpuFluid *fluid);
void gpu_fluid_step(GpuFluid *fluid, float frame_delta);
void gpu_fluid_draw(const GpuFluid *fluid, const Camera *camera, int width, int height);
void gpu_fluid_draw_surface(GpuFluid *fluid, const Camera *camera, int width, int height,
                            float time_seconds, Vec3 light_direction);
const char *gpu_fluid_quality_name(const GpuFluid *fluid);
void gpu_fluid_destroy(GpuFluid *fluid);

#endif
