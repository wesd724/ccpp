#include "object_renderer.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float position[3];
    float normal[3];
} ObjectVertex;

static void set_vec3(GLuint program, const char *name, Vec3 value) {
    glUniform3f(glGetUniformLocation(program, name), value.x, value.y, value.z);
}

static void set_float(GLuint program, const char *name, float value) {
    glUniform1f(glGetUniformLocation(program, name), value);
}

static void create_cube(ObjectRenderer *renderer) {
    static const Vec3 normals[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    static const Vec3 corners[6][4] = {
        {{1,-1,-1},{1,-1,1},{1,1,1},{1,1,-1}},
        {{-1,-1,1},{-1,-1,-1},{-1,1,-1},{-1,1,1}},
        {{-1,1,-1},{1,1,-1},{1,1,1},{-1,1,1}},
        {{-1,-1,1},{1,-1,1},{1,-1,-1},{-1,-1,-1}},
        {{1,-1,1},{-1,-1,1},{-1,1,1},{1,1,1}},
        {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1}},
    };
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    ObjectVertex vertices[36];
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i < 6; ++i) {
            Vec3 p = corners[face][order[i]];
            Vec3 n = normals[face];
            vertices[face * 6 + i] = (ObjectVertex){{p.x, p.y, p.z}, {n.x, n.y, n.z}};
        }
    }
    glGenVertexArrays(1, &renderer->cube_vao);
    glBindVertexArray(renderer->cube_vao);
    glGenBuffers(1, &renderer->cube_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ObjectVertex),
                          (const void *)offsetof(ObjectVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ObjectVertex),
                          (const void *)offsetof(ObjectVertex, normal));
}

static void create_cloth(ObjectRenderer *renderer) {
    uint32_t indices[(CLOTH_COLUMNS - 1) * (CLOTH_ROWS - 1) * 6];
    size_t cursor = 0;
    for (int z = 0; z < CLOTH_ROWS - 1; ++z) {
        for (int x = 0; x < CLOTH_COLUMNS - 1; ++x) {
            uint32_t a = (uint32_t)(z * CLOTH_COLUMNS + x);
            uint32_t b = a + 1;
            uint32_t c = a + CLOTH_COLUMNS;
            uint32_t d = c + 1;
            indices[cursor++] = a; indices[cursor++] = c; indices[cursor++] = b;
            indices[cursor++] = b; indices[cursor++] = c; indices[cursor++] = d;
        }
    }
    renderer->cloth_index_count = (GLsizei)cursor;
    glGenVertexArrays(1, &renderer->cloth_vao);
    glBindVertexArray(renderer->cloth_vao);
    glGenBuffers(1, &renderer->cloth_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->cloth_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ObjectVertex) * CLOTH_NODE_COUNT, NULL, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &renderer->cloth_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->cloth_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ObjectVertex),
                          (const void *)offsetof(ObjectVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ObjectVertex),
                          (const void *)offsetof(ObjectVertex, normal));
}

bool object_renderer_init(ObjectRenderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));
    renderer->object_program = gl_program_from_files("shaders/object.vert", "shaders/object.frag");
    renderer->ui_program = gl_program_from_files("shaders/ui.vert", "shaders/ui.frag");
    if (!renderer->object_program || !renderer->ui_program) {
        object_renderer_destroy(renderer);
        return false;
    }
    create_cube(renderer);
    create_cloth(renderer);
    glGenVertexArrays(1, &renderer->ui_vao);
    glBindVertexArray(0);
    return true;
}

static void draw_box(ObjectRenderer *renderer, Vec3 position, Vec3 half_extents,
                     Vec3 rotation, Vec3 color, float alpha, float selected) {
    set_vec3(renderer->object_program, "u_center", position);
    set_vec3(renderer->object_program, "u_half_extent", half_extents);
    set_vec3(renderer->object_program, "u_rotation", rotation);
    set_vec3(renderer->object_program, "u_color", color);
    set_float(renderer->object_program, "u_alpha", alpha);
    set_float(renderer->object_program, "u_selected", selected);
    glBindVertexArray(renderer->cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void build_cloth_vertices(const PhysicsCloth *cloth, ObjectVertex *vertices) {
    Vec3 normals[CLOTH_NODE_COUNT];
    memset(normals, 0, sizeof(normals));
    for (int z = 0; z < CLOTH_ROWS - 1; ++z) {
        for (int x = 0; x < CLOTH_COLUMNS - 1; ++x) {
            int a = z * CLOTH_COLUMNS + x;
            int b = a + 1;
            int c = a + CLOTH_COLUMNS;
            int d = c + 1;
            Vec3 n0 = vec3_cross(vec3_sub(cloth->nodes[c].position, cloth->nodes[a].position),
                                 vec3_sub(cloth->nodes[b].position, cloth->nodes[a].position));
            Vec3 n1 = vec3_cross(vec3_sub(cloth->nodes[c].position, cloth->nodes[b].position),
                                 vec3_sub(cloth->nodes[d].position, cloth->nodes[b].position));
            normals[a] = vec3_add(normals[a], n0);
            normals[b] = vec3_add(normals[b], vec3_add(n0, n1));
            normals[c] = vec3_add(normals[c], vec3_add(n0, n1));
            normals[d] = vec3_add(normals[d], n1);
        }
    }
    for (int i = 0; i < CLOTH_NODE_COUNT; ++i) {
        Vec3 p = cloth->nodes[i].position;
        Vec3 n = vec3_normalize(normals[i]);
        if (vec3_dot(n, n) < 0.1f) n = vec3(0, 1, 0);
        vertices[i] = (ObjectVertex){{p.x, p.y, p.z}, {n.x, n.y, n.z}};
    }
}

static void draw_cloth(ObjectRenderer *renderer, const PhysicsCloth *cloth, bool selected) {
    ObjectVertex vertices[CLOTH_NODE_COUNT];
    build_cloth_vertices(cloth, vertices);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->cloth_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    set_vec3(renderer->object_program, "u_center", vec3(0, 0, 0));
    set_vec3(renderer->object_program, "u_half_extent", vec3(1, 1, 1));
    set_vec3(renderer->object_program, "u_rotation", vec3(0, 0, 0));
    set_vec3(renderer->object_program, "u_color", cloth->color);
    set_float(renderer->object_program, "u_alpha", 0.88f);
    set_float(renderer->object_program, "u_selected", selected ? 1.0f : 0.0f);
    glBindVertexArray(renderer->cloth_vao);
    glDrawElements(GL_TRIANGLES, renderer->cloth_index_count, GL_UNSIGNED_INT, NULL);
}

static Vec3 prefab_color(PhysicsPrefab prefab) {
    static const Vec3 colors[] = {
        {0.43f, 0.20f, 0.07f}, {0.72f, 0.49f, 0.23f}, {0.72f, 0.08f, 0.055f},
        {0.48f, 0.84f, 0.90f}, {0.92f, 0.16f, 0.04f}, {0.51f, 0.29f, 0.09f}
    };
    return colors[prefab];
}

void object_renderer_draw_world(ObjectRenderer *renderer, const PhysicsWorld *world,
                                Mat4 view_projection, Vec3 camera_position) {
    glUseProgram(renderer->object_program);
    glUniformMatrix4fv(glGetUniformLocation(renderer->object_program, "u_view_projection"),
                       1, GL_FALSE, view_projection.m);
    set_vec3(renderer->object_program, "u_camera", camera_position);
    set_vec3(renderer->object_program, "u_light_direction",
             vec3_normalize(vec3(-0.32f, -0.82f, -0.46f)));
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        const PhysicsBody *body = &world->bodies[i];
        if (!body->active) continue;
        draw_box(renderer, body->position, body->half_extents, body->rotation,
                 body->color, body->alpha, body->group_id == world->selected_group ? 1.0f : 0.0f);
    }
    for (int i = 0; i < PHYSICS_MAX_CLOTHS; ++i) {
        const PhysicsCloth *cloth = &world->cloths[i];
        if (cloth->active) draw_cloth(renderer, cloth, cloth->group_id == world->selected_group);
    }
    if (world->placing && world->placement_valid) {
        Vec3 half = vec3(0.9f, 0.7f, 0.9f);
        if (world->selected_prefab == PHYSICS_PREFAB_SANDCASTLE) half = vec3(1.8f, 1.2f, 1.4f);
        if (world->selected_prefab == PHYSICS_PREFAB_CLOTH) half = vec3(1.7f, 0.05f, 1.2f);
        if (world->selected_prefab == PHYSICS_PREFAB_GLASS) half = vec3(1.3f, 1.6f, 0.08f);
        if (world->selected_prefab == PHYSICS_PREFAB_LEGO) half = vec3(1.8f, 1.8f, 0.5f);
        if (world->selected_prefab == PHYSICS_PREFAB_RAFT) half = vec3(2.2f, 0.2f, 2.1f);
        Vec3 center = vec3_add(world->placement_position, vec3(0, half.y + 0.18f, 0));
        draw_box(renderer, center, half, vec3(0, 0, 0), prefab_color(world->selected_prefab), 0.42f, 1.0f);
    }
}

static void draw_ui_rect(ObjectRenderer *renderer, float left, float top, float right, float bottom,
                         Vec3 color, float selected, float icon) {
    glUniform4f(glGetUniformLocation(renderer->ui_program, "u_rect"), left, top, right, bottom);
    set_vec3(renderer->ui_program, "u_color", color);
    set_float(renderer->ui_program, "u_selected", selected);
    set_float(renderer->ui_program, "u_icon", icon);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void object_renderer_draw_ui(ObjectRenderer *renderer, const PhysicsWorld *world,
                             int width, int height) {
    if (width <= 0 || height <= 0) return;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(renderer->ui_program);
    glBindVertexArray(renderer->ui_vao);
    for (int i = 0; i < PHYSICS_PREFAB_COUNT; ++i) {
        float x0 = -1.0f + 2.0f * 12.0f / width;
        float x1 = -1.0f + 2.0f * 88.0f / width;
        float y0_pixels = 78.0f + i * 73.0f;
        float y1_pixels = y0_pixels + 66.0f;
        float y0 = 1.0f - 2.0f * y0_pixels / height;
        float y1 = 1.0f - 2.0f * y1_pixels / height;
        draw_ui_rect(renderer, x0, y0, x1, y1, prefab_color((PhysicsPrefab)i),
                     world->selected_prefab == (PhysicsPrefab)i ? 1.0f : 0.0f, (float)i);
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void object_renderer_destroy(ObjectRenderer *renderer) {
    if (renderer->object_program) glDeleteProgram(renderer->object_program);
    if (renderer->ui_program) glDeleteProgram(renderer->ui_program);
    GLuint buffers[] = {renderer->cube_vbo, renderer->cloth_vbo, renderer->cloth_ebo};
    glDeleteBuffers(3, buffers);
    GLuint arrays[] = {renderer->cube_vao, renderer->cloth_vao, renderer->ui_vao};
    glDeleteVertexArrays(3, arrays);
    memset(renderer, 0, sizeof(*renderer));
}
