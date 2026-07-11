#include "renderer.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float position[3];
} FloorVertex;

static bool create_water_mesh(Renderer *renderer, const FluidSim *sim) {
    size_t index_count = (size_t)FLUID_CELL_COUNT * 6;
    uint32_t *indices = malloc(sizeof(*indices) * index_count);
    if (!indices) return false;

    size_t index = 0;
    for (int z = 0; z < FLUID_RESOLUTION; ++z) {
        for (int x = 0; x < FLUID_RESOLUTION; ++x) {
            uint32_t a = (uint32_t)(z * FLUID_GRID_WIDTH + x);
            uint32_t b = a + 1;
            uint32_t c = a + FLUID_GRID_WIDTH;
            uint32_t d = c + 1;
            indices[index++] = a;
            indices[index++] = c;
            indices[index++] = b;
            indices[index++] = b;
            indices[index++] = c;
            indices[index++] = d;
        }
    }

    renderer->water_index_count = (GLsizei)index_count;
    glGenVertexArrays(1, &renderer->water_vao);
    glBindVertexArray(renderer->water_vao);
    glGenBuffers(1, &renderer->water_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->water_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(FluidVertex) * FLUID_VERTEX_COUNT,
                 fluid_sim_vertices(sim), GL_DYNAMIC_DRAW);
    glGenBuffers(1, &renderer->water_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->water_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(*indices) * index_count),
                 indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FluidVertex),
                          (const void *)offsetof(FluidVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FluidVertex),
                          (const void *)offsetof(FluidVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(FluidVertex),
                          (const void *)offsetof(FluidVertex, foam));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(FluidVertex),
                          (const void *)offsetof(FluidVertex, velocity));
    glBindVertexArray(0);
    free(indices);
    return true;
}

static void create_floor_mesh(Renderer *renderer) {
    const FloorVertex vertices[] = {
        {{-82.0f, -2.35f, -82.0f}}, {{82.0f, -2.35f, -82.0f}},
        {{-82.0f, -2.35f, 82.0f}},  {{82.0f, -2.35f, 82.0f}},
    };
    glGenVertexArrays(1, &renderer->floor_vao);
    glBindVertexArray(renderer->floor_vao);
    glGenBuffers(1, &renderer->floor_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->floor_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FloorVertex), (const void *)0);
    glBindVertexArray(0);
}

static void create_spray_buffer(Renderer *renderer) {
    glGenVertexArrays(1, &renderer->spray_vao);
    glBindVertexArray(renderer->spray_vao);
    glGenBuffers(1, &renderer->spray_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->spray_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(SprayRenderVertex) * FLUID_MAX_SPRAY, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SprayRenderVertex),
                          (const void *)offsetof(SprayRenderVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SprayRenderVertex),
                          (const void *)offsetof(SprayRenderVertex, alpha));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(SprayRenderVertex),
                          (const void *)offsetof(SprayRenderVertex, size));
    glBindVertexArray(0);
}

bool renderer_init(Renderer *renderer, const FluidSim *sim) {
    memset(renderer, 0, sizeof(*renderer));
    renderer->water_program = gl_program_from_files("shaders/water.vert", "shaders/water.frag");
    renderer->floor_program = gl_program_from_files("shaders/floor.vert", "shaders/floor.frag");
    renderer->sky_program = gl_program_from_files("shaders/sky.vert", "shaders/sky.frag");
    renderer->spray_program = gl_program_from_files("shaders/spray.vert", "shaders/spray.frag");
    renderer->stage_program = gl_program_from_files("shaders/stage.vert", "shaders/stage.frag");
    renderer->material_sky_program = gl_program_from_files(
        "shaders/sky.vert", "shaders/material_sky.frag");
    renderer->material_stage_program = gl_program_from_files(
        "shaders/stage.vert", "shaders/material_stage.frag");
    if (!renderer->water_program || !renderer->floor_program || !renderer->sky_program ||
        !renderer->spray_program || !renderer->stage_program ||
        !renderer->material_sky_program || !renderer->material_stage_program ||
        !create_water_mesh(renderer, sim) ||
        !object_renderer_init(&renderer->objects) || !hud_renderer_init(&renderer->hud)) {
        renderer_destroy(renderer);
        return false;
    }
    renderer->spray_staging = malloc(sizeof(*renderer->spray_staging) * FLUID_MAX_SPRAY);
    if (!renderer->spray_staging) {
        renderer_destroy(renderer);
        return false;
    }
    create_floor_mesh(renderer);
    glGenVertexArrays(1, &renderer->sky_vao);
    create_spray_buffer(renderer);
    return true;
}

static void set_matrix(GLuint program, const char *name, Mat4 matrix) {
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, matrix.m);
}

static void set_vec3(GLuint program, const char *name, Vec3 value) {
    glUniform3f(glGetUniformLocation(program, name), value.x, value.y, value.z);
}

static void draw_spray(Renderer *renderer, const FluidSim *sim, Mat4 view_projection) {
    size_t count = 0;
    const SprayParticle *particles = fluid_sim_spray(sim, &count);
    if (!count) return;
    SprayRenderVertex *vertices = renderer->spray_staging;
    for (size_t i = 0; i < count; ++i) {
        vertices[i].position[0] = particles[i].position.x;
        vertices[i].position[1] = particles[i].position.y;
        vertices[i].position[2] = particles[i].position.z;
        float normalized_life = particles[i].life / particles[i].max_life;
        vertices[i].alpha = clampf(normalized_life * 1.8f, 0.0f, 1.0f);
        vertices[i].size = particles[i].size;
    }
    glBindBuffer(GL_ARRAY_BUFFER, renderer->spray_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(*vertices) * count), vertices, GL_DYNAMIC_DRAW);

    glUseProgram(renderer->spray_program);
    set_matrix(renderer->spray_program, "u_view_projection", view_projection);
    glBindVertexArray(renderer->spray_vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)count);
}

void renderer_draw(Renderer *renderer, FluidSim *sim, const PhysicsWorld *world, const Camera *camera,
                   int width, int height, float time_seconds, float interpolation) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.55f, 0.73f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)width / (float)height;
    Mat4 view_projection = camera_view_projection(camera, aspect);
    Vec3 light_direction = vec3_normalize(vec3(-0.32f, -0.82f, -0.46f));

    glDepthMask(GL_FALSE);
    glUseProgram(renderer->sky_program);
    glUniform1f(glGetUniformLocation(renderer->sky_program, "u_aspect"), aspect);
    set_vec3(renderer->sky_program, "u_view_direction", camera_forward(camera));
    set_vec3(renderer->sky_program, "u_sun_direction", vec3_scale(light_direction, -1.0f));
    glUniform1f(glGetUniformLocation(renderer->sky_program, "u_exposure"), 1.0f);
    glBindVertexArray(renderer->sky_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);

    glUseProgram(renderer->floor_program);
    set_matrix(renderer->floor_program, "u_view_projection", view_projection);
    glUniform1f(glGetUniformLocation(renderer->floor_program, "u_time"), time_seconds);
    glBindVertexArray(renderer->floor_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    object_renderer_draw_world(&renderer->objects, world, view_projection, camera->position);

    fluid_sim_update_vertices(sim, interpolation);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->water_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(FluidVertex) * FLUID_VERTEX_COUNT,
                 fluid_sim_vertices(sim), GL_DYNAMIC_DRAW);
    glUseProgram(renderer->water_program);
    set_matrix(renderer->water_program, "u_view_projection", view_projection);
    set_vec3(renderer->water_program, "u_camera", camera->position);
    set_vec3(renderer->water_program, "u_light_direction", light_direction);
    glUniform1f(glGetUniformLocation(renderer->water_program, "u_time"), time_seconds);
    glBindVertexArray(renderer->water_vao);
    glDrawElements(GL_TRIANGLES, renderer->water_index_count, GL_UNSIGNED_INT, NULL);

    draw_spray(renderer, sim, view_projection);
    object_renderer_draw_ui(&renderer->objects, world, width, height);
    glBindVertexArray(0);
}

static const PhysicsWorld *cinematic_obstacles(void) {
    static PhysicsWorld obstacles;
    static bool obstacles_ready;
    if (!obstacles_ready) {
        physics_world_init(&obstacles);
        for (int i = 0; i < 5; ++i) {
            PhysicsBody *body = &obstacles.bodies[i];
            body->active = true;
            body->group_id = i + 1;
            float top = -0.1f + (float)i * 0.52f;
            body->position = vec3(-2.2f + (float)i * 1.1f,
                                  (-2.30f + top) * 0.5f,
                                  sinf((float)i * 1.35f) * 0.48f);
            body->half_extents = vec3(0.34f, (top + 2.30f) * 0.5f, 0.34f);
            body->color = vec3(0.34f, 0.39f, 0.44f);
            body->alpha = 1.0f;
        }
        obstacles.selected_group = -1;
        obstacles_ready = true;
    }
    return &obstacles;
}

static Vec3 draw_cinematic_scene(Renderer *renderer, const Camera *camera,
                                 int width, int height, float time_seconds) {
    if (width <= 0 || height <= 0) return vec3(0.0f, -1.0f, 0.0f);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.30f, 0.48f, 0.66f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)width / (float)height;
    Mat4 view_projection = camera_view_projection(camera, aspect);
    Vec3 light_direction = vec3_normalize(vec3(-0.32f, -0.82f, -0.46f));
    glDepthMask(GL_FALSE);
    glUseProgram(renderer->sky_program);
    glUniform1f(glGetUniformLocation(renderer->sky_program, "u_aspect"), aspect);
    set_vec3(renderer->sky_program, "u_view_direction", camera_forward(camera));
    set_vec3(renderer->sky_program, "u_sun_direction", vec3_scale(light_direction, -1.0f));
    glUniform1f(glGetUniformLocation(renderer->sky_program, "u_exposure"), 1.42f);
    glBindVertexArray(renderer->sky_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);

    glUseProgram(renderer->stage_program);
    set_matrix(renderer->stage_program, "u_view_projection", view_projection);
    glUniform1f(glGetUniformLocation(renderer->stage_program, "u_time"), time_seconds);
    set_vec3(renderer->stage_program, "u_light_direction", light_direction);
    glBindVertexArray(renderer->floor_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    object_renderer_draw_world(&renderer->objects, cinematic_obstacles(),
                               view_projection, camera->position);
    return light_direction;
}

void renderer_draw_cinematic(Renderer *renderer, const GpuFluid *fluid, const Camera *camera,
                             int width, int height, float time_seconds) {
    if (width <= 0 || height <= 0) return;
    draw_cinematic_scene(renderer, camera, width, height, time_seconds);
    gpu_fluid_draw(fluid, camera, width, height);
    glBindVertexArray(0);
}

void renderer_draw_high_quality(Renderer *renderer, GpuFluid *fluid, const Camera *camera,
                                int width, int height, float time_seconds) {
    if (width <= 0 || height <= 0) return;
    Vec3 light_direction = draw_cinematic_scene(renderer, camera, width, height, time_seconds);
    gpu_fluid_draw_surface(fluid, camera, width, height, time_seconds, light_direction);
    glBindVertexArray(0);
}

static const PhysicsWorld *material_lab_rotors(float time_seconds) {
    static PhysicsWorld rotors;
    static bool initialized;
    if (!initialized) {
        physics_world_init(&rotors);
        rotors.selected_group = -1;
        initialized = true;
    }
    for (int i = 0; i < 20; ++i) rotors.bodies[i].active = false;
    int body_index = 0;
    const float centers[2] = {-1.35f, 1.35f};
    const float omegas[2] = {-2.65f, 2.65f};
    const float phases[2] = {0.0f, 3.14159265358979323846f};
    for (int rotor = 0; rotor < 2; ++rotor) {
        float angle = omegas[rotor] * time_seconds + phases[rotor];
        PhysicsBody *core = &rotors.bodies[body_index++];
        core->active = true;
        core->group_id = 100 + body_index;
        core->position = vec3(centers[rotor], -0.72f, 0.0f);
        core->half_extents = vec3(0.49f, 0.49f, 2.38f);
        core->rotation = vec3(0.0f, 0.0f, angle);
        core->color = vec3(0.32f, 0.36f, 0.42f);
        core->alpha = 1.0f;
        for (int arm = 0; arm < 5; ++arm) {
            float arm_angle = angle + (float)arm *
                              (2.0f * 3.14159265358979323846f / 5.0f);
            PhysicsBody *paddle = &rotors.bodies[body_index++];
            paddle->active = true;
            paddle->group_id = 100 + body_index;
            paddle->position = vec3(centers[rotor] + cosf(arm_angle) * 0.72f,
                                    -0.72f + sinf(arm_angle) * 0.72f, 0.0f);
            paddle->half_extents = vec3(0.72f, 0.15f, 2.28f);
            paddle->rotation = vec3(0.0f, 0.0f, arm_angle);
            paddle->color = rotor == 0 ? vec3(0.68f, 0.74f, 0.82f) :
                                         vec3(0.82f, 0.68f, 0.50f);
            paddle->alpha = 1.0f;
        }
    }
    return &rotors;
}

void renderer_draw_material_lab(Renderer *renderer, const GpuMpm *mpm, const Camera *camera,
                                int width, int height, float time_seconds) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.48f, 0.53f, 0.60f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float aspect = (float)width / (float)height;
    Mat4 view_projection = camera_view_projection(camera, aspect);
    Vec3 light_direction = vec3_normalize(vec3(-0.32f, -0.82f, -0.46f));

    glDepthMask(GL_FALSE);
    glUseProgram(renderer->material_sky_program);
    glUniform1f(glGetUniformLocation(renderer->material_sky_program, "u_aspect"), aspect);
    set_vec3(renderer->material_sky_program, "u_view_direction", camera_forward(camera));
    set_vec3(renderer->material_sky_program, "u_sun_direction",
             vec3_scale(light_direction, -1.0f));
    glBindVertexArray(renderer->sky_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);

    glUseProgram(renderer->material_stage_program);
    set_matrix(renderer->material_stage_program, "u_view_projection", view_projection);
    glUniform1f(glGetUniformLocation(renderer->material_stage_program, "u_time"), time_seconds);
    glBindVertexArray(renderer->floor_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    gpu_mpm_draw_shadows(mpm, camera, width, height);
    object_renderer_draw_world(&renderer->objects, material_lab_rotors(gpu_mpm_time(mpm)),
                               view_projection, camera->position);
    gpu_mpm_draw_particles(mpm, camera, width, height);
    glBindVertexArray(0);
}

void renderer_draw_mode_hud(Renderer *renderer, HudMode mode, int width, int height) {
    hud_renderer_draw(&renderer->hud, mode, width, height);
}

void renderer_destroy(Renderer *renderer) {
    hud_renderer_destroy(&renderer->hud);
    object_renderer_destroy(&renderer->objects);
    GLuint programs[] = {renderer->water_program, renderer->floor_program,
                         renderer->sky_program, renderer->spray_program,
                         renderer->stage_program, renderer->material_sky_program,
                         renderer->material_stage_program};
    for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); ++i) {
        if (programs[i]) glDeleteProgram(programs[i]);
    }
    GLuint buffers[] = {renderer->water_vbo, renderer->water_ebo,
                        renderer->floor_vbo, renderer->spray_vbo};
    glDeleteBuffers((GLsizei)(sizeof(buffers) / sizeof(buffers[0])), buffers);
    GLuint arrays[] = {renderer->water_vao, renderer->floor_vao,
                       renderer->sky_vao, renderer->spray_vao};
    glDeleteVertexArrays((GLsizei)(sizeof(arrays) / sizeof(arrays[0])), arrays);
    free(renderer->spray_staging);
    memset(renderer, 0, sizeof(*renderer));
}
