#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "fluid_sim.h"
#include "gl_api.h"
#include "gpu_fluid.h"
#include "gpu_mpm.h"
#include "hud_renderer.h"
#include "object_renderer.h"
#include "physics_world.h"

#include <stdbool.h>

typedef struct {
    float position[3];
    float alpha;
    float size;
} SprayRenderVertex;

typedef struct {
    GLuint water_program;
    GLuint floor_program;
    GLuint sky_program;
    GLuint spray_program;
    GLuint stage_program;
    GLuint material_sky_program;
    GLuint material_stage_program;
    GLuint water_vao;
    GLuint water_vbo;
    GLuint water_ebo;
    GLuint floor_vao;
    GLuint floor_vbo;
    GLuint sky_vao;
    GLuint spray_vao;
    GLuint spray_vbo;
    SprayRenderVertex *spray_staging;
    GLsizei water_index_count;
    ObjectRenderer objects;
    HudRenderer hud;
} Renderer;

bool renderer_init(Renderer *renderer, const FluidSim *sim);
void renderer_draw(Renderer *renderer, FluidSim *sim, const PhysicsWorld *world, const Camera *camera,
                   int width, int height, float time_seconds, float interpolation);
void renderer_draw_cinematic(Renderer *renderer, const GpuFluid *fluid, const Camera *camera,
                             int width, int height, float time_seconds);
void renderer_draw_high_quality(Renderer *renderer, GpuFluid *fluid, const Camera *camera,
                                int width, int height, float time_seconds);
void renderer_draw_material_lab(Renderer *renderer, const GpuMpm *mpm, const Camera *camera,
                                int width, int height, float time_seconds);
void renderer_draw_mode_hud(Renderer *renderer, HudMode mode, int width, int height);
void renderer_destroy(Renderer *renderer);

#endif
