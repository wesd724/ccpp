#include "gpu_surface.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846f

static bool framebuffer_complete(const char *name) {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) return true;
    fprintf(stderr, "%s framebuffer incomplete: 0x%x\n", name, (unsigned)status);
    return false;
}

static void allocate_texture(GLuint *texture, GLint internal_format, GLenum format,
                             GLenum type, int width, int height, GLint filter) {
    if (!*texture) glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, NULL);
}

static bool resize_targets(GpuSurface *surface, int width, int height) {
    if (surface->width == width && surface->height == height) return true;
    allocate_texture(&surface->linear_depth_texture, GL_R32F, GL_RED, GL_FLOAT,
                     width, height, GL_LINEAR);
    allocate_texture(&surface->hardware_depth_texture, GL_DEPTH_COMPONENT32F,
                     GL_DEPTH_COMPONENT, GL_FLOAT, width, height, GL_NEAREST);
    allocate_texture(&surface->thickness_texture, GL_RG16F, GL_RG, GL_FLOAT,
                     width, height, GL_LINEAR);
    allocate_texture(&surface->scene_texture, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
                     width, height, GL_LINEAR);
    for (int i = 0; i < 2; ++i) {
        allocate_texture(&surface->blur_textures[i], GL_R32F, GL_RED, GL_FLOAT,
                         width, height, GL_LINEAR);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, surface->depth_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           surface->linear_depth_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           surface->hardware_depth_texture, 0);
    bool complete = framebuffer_complete("fluid depth");

    glBindFramebuffer(GL_FRAMEBUFFER, surface->thickness_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           surface->thickness_texture, 0);
    complete = framebuffer_complete("fluid thickness") && complete;

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, surface->blur_fbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               surface->blur_textures[i], 0);
        complete = framebuffer_complete("fluid depth blur") && complete;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    surface->width = width;
    surface->height = height;
    return complete;
}

bool gpu_surface_init(GpuSurface *surface) {
    memset(surface, 0, sizeof(*surface));
    surface->depth_program = gl_program_from_files(
        "shaders/fluid_splat.vert", "shaders/fluid_depth.frag");
    surface->thickness_program = gl_program_from_files(
        "shaders/fluid_splat.vert", "shaders/fluid_thickness.frag");
    surface->blur_program = gl_program_from_files(
        "shaders/fluid_fullscreen.vert", "shaders/fluid_depth_blur.frag");
    surface->composite_program = gl_program_from_files(
        "shaders/fluid_fullscreen.vert", "shaders/fluid_surface.frag");
    if (!surface->depth_program || !surface->thickness_program ||
        !surface->blur_program || !surface->composite_program) {
        gpu_surface_destroy(surface);
        return false;
    }
    glGenFramebuffers(1, &surface->depth_fbo);
    glGenFramebuffers(1, &surface->thickness_fbo);
    glGenFramebuffers(2, surface->blur_fbos);
    glGenVertexArrays(1, &surface->fullscreen_vao);
    surface->initialized = true;
    return true;
}

static void set_splat_uniforms(GLuint program, Mat4 view, Mat4 projection,
                               float point_scale, float particle_radius) {
    glUniformMatrix4fv(glGetUniformLocation(program, "u_view"),
                       1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_projection"),
                       1, GL_FALSE, projection.m);
    glUniform1f(glGetUniformLocation(program, "u_point_scale"), point_scale);
    glUniform1f(glGetUniformLocation(program, "u_particle_radius"), particle_radius);
}

static void draw_fullscreen(GLuint vao) {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void gpu_surface_draw(GpuSurface *surface, GLuint particle_vao, size_t particle_count,
                      float particle_radius, const Camera *camera, int width, int height,
                      float time_seconds, Vec3 light_direction) {
    if (!surface->initialized || width <= 0 || height <= 0 ||
        !resize_targets(surface, width, height)) return;

    float aspect = (float)width / (float)height;
    Mat4 view = camera_view_matrix(camera);
    Mat4 projection = camera_projection_matrix(camera, aspect);
    Mat4 view_projection = mat4_multiply(projection, view);
    float point_scale = (float)height * projection.m[5];

    glViewport(0, 0, width, height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, surface->scene_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, surface->depth_fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(surface->depth_program);
    set_splat_uniforms(surface->depth_program, view, projection,
                       point_scale, particle_radius);
    glBindVertexArray(particle_vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)particle_count);

    glBindFramebuffer(GL_FRAMEBUFFER, surface->thickness_fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(surface->thickness_program);
    set_splat_uniforms(surface->thickness_program, view, projection,
                       point_scale, particle_radius * 1.18f);
    glBindVertexArray(particle_vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)particle_count);
    glDisable(GL_BLEND);

    glUseProgram(surface->blur_program);
    glUniform1i(glGetUniformLocation(surface->blur_program, "u_depth"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, surface->blur_fbos[0]);
    glBindTexture(GL_TEXTURE_2D, surface->linear_depth_texture);
    glUniform1i(glGetUniformLocation(surface->blur_program, "u_horizontal"), 1);
    draw_fullscreen(surface->fullscreen_vao);

    glBindFramebuffer(GL_FRAMEBUFFER, surface->blur_fbos[1]);
    glBindTexture(GL_TEXTURE_2D, surface->blur_textures[0]);
    glUniform1i(glGetUniformLocation(surface->blur_program, "u_horizontal"), 0);
    draw_fullscreen(surface->fullscreen_vao);

    Vec3 forward = camera_forward(camera);
    Vec3 right = vec3_normalize(vec3_cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    Vec3 up = vec3_normalize(vec3_cross(right, forward));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(surface->composite_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, surface->blur_textures[1]);
    glUniform1i(glGetUniformLocation(surface->composite_program, "u_depth"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, surface->thickness_texture);
    glUniform1i(glGetUniformLocation(surface->composite_program, "u_thickness"), 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, surface->scene_texture);
    glUniform1i(glGetUniformLocation(surface->composite_program, "u_scene"), 2);
    glUniformMatrix4fv(glGetUniformLocation(surface->composite_program,
                                            "u_view_projection"),
                       1, GL_FALSE, view_projection.m);
    glUniform3f(glGetUniformLocation(surface->composite_program, "u_camera"),
                camera->position.x, camera->position.y, camera->position.z);
    glUniform3f(glGetUniformLocation(surface->composite_program, "u_forward"),
                forward.x, forward.y, forward.z);
    glUniform3f(glGetUniformLocation(surface->composite_program, "u_right"),
                right.x, right.y, right.z);
    glUniform3f(glGetUniformLocation(surface->composite_program, "u_up"),
                up.x, up.y, up.z);
    glUniform3f(glGetUniformLocation(surface->composite_program, "u_light_direction"),
                light_direction.x, light_direction.y, light_direction.z);
    glUniform1f(glGetUniformLocation(surface->composite_program, "u_aspect"), aspect);
    glUniform1f(glGetUniformLocation(surface->composite_program, "u_tan_half_fov"),
                tanf(camera->fov_degrees * PI / 360.0f));
    glUniform1f(glGetUniformLocation(surface->composite_program, "u_time"), time_seconds);
    draw_fullscreen(surface->fullscreen_vao);
    glActiveTexture(GL_TEXTURE0);
}

void gpu_surface_destroy(GpuSurface *surface) {
    GLuint programs[] = {surface->depth_program, surface->thickness_program,
                         surface->blur_program, surface->composite_program};
    for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); ++i) {
        if (programs[i]) glDeleteProgram(programs[i]);
    }
    GLuint framebuffers[] = {surface->depth_fbo, surface->thickness_fbo,
                             surface->blur_fbos[0], surface->blur_fbos[1]};
    glDeleteFramebuffers((GLsizei)(sizeof(framebuffers) / sizeof(framebuffers[0])),
                         framebuffers);
    GLuint textures[] = {surface->linear_depth_texture, surface->hardware_depth_texture,
                         surface->thickness_texture, surface->scene_texture,
                         surface->blur_textures[0], surface->blur_textures[1]};
    glDeleteTextures((GLsizei)(sizeof(textures) / sizeof(textures[0])), textures);
    if (surface->fullscreen_vao) glDeleteVertexArrays(1, &surface->fullscreen_vao);
    memset(surface, 0, sizeof(*surface));
}
