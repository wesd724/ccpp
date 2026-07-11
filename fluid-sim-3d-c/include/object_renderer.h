#ifndef OBJECT_RENDERER_H
#define OBJECT_RENDERER_H

#include "gl_api.h"
#include "math3d.h"
#include "physics_world.h"

#include <stdbool.h>

typedef struct {
    GLuint object_program;
    GLuint ui_program;
    GLuint cube_vao;
    GLuint cube_vbo;
    GLuint cloth_vao;
    GLuint cloth_vbo;
    GLuint cloth_ebo;
    GLuint ui_vao;
    GLsizei cloth_index_count;
} ObjectRenderer;

bool object_renderer_init(ObjectRenderer *renderer);
void object_renderer_draw_world(ObjectRenderer *renderer, const PhysicsWorld *world,
                                Mat4 view_projection, Vec3 camera_position);
void object_renderer_draw_ui(ObjectRenderer *renderer, const PhysicsWorld *world,
                             int width, int height);
void object_renderer_destroy(ObjectRenderer *renderer);

#endif
