#ifndef GPU_MPM_H
#define GPU_MPM_H

#include "camera.h"
#include "gl_api.h"

#include <stdbool.h>
#include <stddef.h>

#define GPU_MPM_GRID_X 80
#define GPU_MPM_GRID_Y 72
#define GPU_MPM_GRID_Z 68
#define GPU_MPM_GRID_CELLS (GPU_MPM_GRID_X * GPU_MPM_GRID_Y * GPU_MPM_GRID_Z)
#define GPU_MPM_MAX_PARTICLES 400000
#define GPU_MPM_MAX_IMPULSES 4
#define GPU_MPM_FLOOR_Y (-2.25f)

typedef struct {
    float position[4];
    float velocity[4];
    float deformation[3][4];
    float affine[3][4];
    float state[4];
} GpuMpmParticle;

typedef struct {
    Vec3 center;
    Vec3 radius;
    Vec3 acceleration;
} GpuMpmImpulse;

typedef struct {
    GLuint particle_buffer;
    GLuint particle_vao;
    GLuint grid_accum;
    GLuint grid_velocity;
    GLuint clear_program;
    GLuint p2g_program;
    GLuint grid_program;
    GLuint g2p_program;
    GLuint particle_program;
    GLuint shadow_program;
    GpuMpmImpulse impulses[GPU_MPM_MAX_IMPULSES];
    size_t particle_count;
    int impulse_count;
    float simulation_time;
    bool initialized;
} GpuMpm;

bool gpu_mpm_init(GpuMpm *mpm);
void gpu_mpm_reset(GpuMpm *mpm);
void gpu_mpm_stir(GpuMpm *mpm, Vec3 position, Vec3 drag_velocity);
void gpu_mpm_impact(GpuMpm *mpm);
void gpu_mpm_step(GpuMpm *mpm, float frame_delta);
void gpu_mpm_draw_shadows(const GpuMpm *mpm, const Camera *camera,
                          int width, int height);
void gpu_mpm_draw_particles(const GpuMpm *mpm, const Camera *camera,
                            int width, int height);
float gpu_mpm_time(const GpuMpm *mpm);
void gpu_mpm_destroy(GpuMpm *mpm);

#endif
