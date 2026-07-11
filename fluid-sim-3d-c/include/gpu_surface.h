#ifndef GPU_SURFACE_H
#define GPU_SURFACE_H

#include "camera.h"
#include "gl_api.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    GLuint depth_program;
    GLuint thickness_program;
    GLuint blur_program;
    GLuint composite_program;
    GLuint depth_fbo;
    GLuint thickness_fbo;
    GLuint blur_fbos[2];
    GLuint linear_depth_texture;
    GLuint hardware_depth_texture;
    GLuint thickness_texture;
    GLuint scene_texture;
    GLuint blur_textures[2];
    GLuint fullscreen_vao;
    int width;
    int height;
    bool initialized;
} GpuSurface;

bool gpu_surface_init(GpuSurface *surface);
void gpu_surface_draw(GpuSurface *surface, GLuint particle_vao, size_t particle_count,
                      float particle_radius, const Camera *camera, int width, int height,
                      float time_seconds, Vec3 light_direction);
void gpu_surface_destroy(GpuSurface *surface);

#endif
