#include "gpu_fluid.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_SIZE 256u

static GLuint groups_for(size_t count) {
    return (GLuint)((count + LOCAL_SIZE - 1) / LOCAL_SIZE);
}

static float random_jitter(uint32_t *state, float amplitude) {
    *state = *state * 1664525u + 1013904223u;
    return ((float)(*state >> 8) / 16777216.0f - 0.5f) * amplitude;
}

static void add_block(GpuFluidParticle *particles, size_t *count, uint32_t *random_state,
                      float min_x, float max_x, float min_y, float max_y,
                      float min_z, float max_z, float spacing, size_t maximum_count) {
    float jitter = spacing * 0.115f;
    for (float z = min_z; z <= max_z && *count < maximum_count; z += spacing) {
        for (float y = min_y; y <= max_y && *count < maximum_count; y += spacing) {
            for (float x = min_x; x <= max_x && *count < maximum_count; x += spacing) {
                GpuFluidParticle *particle = &particles[(*count)++];
                particle->position[0] = x + random_jitter(random_state, jitter);
                particle->position[1] = y + random_jitter(random_state, jitter);
                particle->position[2] = z + random_jitter(random_state, jitter);
                particle->position[3] = 1.0f;
                particle->velocity[0] = 0.0f;
                particle->velocity[1] = 0.0f;
                particle->velocity[2] = 0.0f;
                particle->velocity[3] = 0.0f;
            }
        }
    }
}

static GpuFluidParticle *make_initial_particles(const GpuFluid *fluid,
                                                 size_t *particle_count) {
    GpuFluidParticle *particles = calloc(fluid->max_particles, sizeof(*particles));
    if (!particles) return NULL;
    uint32_t random_state = 0x51f15e5du;
    size_t count = 0;
    float spacing = fluid->particle_spacing;
    add_block(particles, &count, &random_state, -6.3f, 6.3f, -2.20f, -1.58f,
              -5.0f, 5.0f, spacing, fluid->max_particles);
    add_block(particles, &count, &random_state, -6.2f, -2.1f, -1.48f, 3.65f,
              -4.9f, 4.9f, spacing, fluid->max_particles);
    add_block(particles, &count, &random_state, 2.1f, 6.2f, -1.48f, 3.65f,
              -4.9f, 4.9f, spacing, fluid->max_particles);
    *particle_count = count;
    return particles;
}

static void configure_quality(GpuFluid *fluid, GpuFluidQuality quality) {
    fluid->quality = quality;
    if (quality == GPU_FLUID_HIGH_QUALITY) {
        fluid->grid_x = 88;
        fluid->grid_y = 58;
        fluid->grid_z = 88;
        fluid->max_particles = 960000;
        fluid->particle_spacing = 0.085f;
        fluid->particle_radius = 0.112f;
        fluid->pressure_iterations = 80;
        fluid->maximum_step = 1.0f / 300.0f;
    } else {
        fluid->grid_x = 48;
        fluid->grid_y = 32;
        fluid->grid_z = 48;
        fluid->max_particles = 180000;
        fluid->particle_spacing = 0.16f;
        fluid->particle_radius = 0.10f;
        fluid->pressure_iterations = 36;
        fluid->maximum_step = 1.0f / 240.0f;
    }
    fluid->dx = 16.0f / (float)fluid->grid_x;
    fluid->grid_cells = (size_t)fluid->grid_x * (size_t)fluid->grid_y *
                        (size_t)fluid->grid_z;
}

static bool load_programs(GpuFluid *fluid) {
    fluid->clear_program = gl_compute_program_from_file("shaders/flip_clear.comp");
    fluid->p2g_program = gl_compute_program_from_file("shaders/flip_p2g.comp");
    fluid->normalize_program = gl_compute_program_from_file("shaders/flip_normalize.comp");
    fluid->force_program = gl_compute_program_from_file("shaders/flip_force.comp");
    fluid->divergence_program = gl_compute_program_from_file("shaders/flip_divergence.comp");
    fluid->pressure_program = gl_compute_program_from_file("shaders/flip_pressure.comp");
    fluid->project_program = gl_compute_program_from_file("shaders/flip_project.comp");
    fluid->g2p_program = gl_compute_program_from_file("shaders/flip_g2p.comp");
    fluid->render_program = gl_program_from_files("shaders/flip_particle.vert", "shaders/flip_particle.frag");
    bool loaded = fluid->clear_program && fluid->p2g_program &&
                  fluid->normalize_program && fluid->force_program &&
                  fluid->divergence_program && fluid->pressure_program &&
                  fluid->project_program && fluid->g2p_program && fluid->render_program;
    if (loaded && fluid->quality == GPU_FLUID_HIGH_QUALITY) {
        loaded = gpu_surface_init(&fluid->surface);
    }
    return loaded;
}

static void allocate_buffer(GLuint *buffer, GLsizeiptr size, const void *data) {
    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

bool gpu_fluid_init(GpuFluid *fluid, GpuFluidQuality quality) {
    memset(fluid, 0, sizeof(*fluid));
    configure_quality(fluid, quality);
    if (!load_programs(fluid)) {
        gpu_fluid_destroy(fluid);
        return false;
    }
    size_t particle_count = 0;
    GpuFluidParticle *particles = make_initial_particles(fluid, &particle_count);
    if (!particles) {
        gpu_fluid_destroy(fluid);
        return false;
    }
    fluid->particle_count = particle_count;
    allocate_buffer(&fluid->particle_buffers[0],
                    (GLsizeiptr)(sizeof(*particles) * fluid->max_particles), particles);
    allocate_buffer(&fluid->particle_buffers[1],
                    (GLsizeiptr)(sizeof(*particles) * fluid->max_particles), particles);
    free(particles);

    allocate_buffer(&fluid->grid_accum,
                    (GLsizeiptr)(sizeof(int32_t) * 4 * fluid->grid_cells), NULL);
    allocate_buffer(&fluid->grid_velocity,
                    (GLsizeiptr)(sizeof(float) * 4 * fluid->grid_cells), NULL);
    allocate_buffer(&fluid->grid_velocity_old,
                    (GLsizeiptr)(sizeof(float) * 4 * fluid->grid_cells), NULL);
    allocate_buffer(&fluid->divergence,
                    (GLsizeiptr)(sizeof(float) * fluid->grid_cells), NULL);
    allocate_buffer(&fluid->pressure[0],
                    (GLsizeiptr)(sizeof(float) * fluid->grid_cells), NULL);
    allocate_buffer(&fluid->pressure[1],
                    (GLsizeiptr)(sizeof(float) * fluid->grid_cells), NULL);

    for (int i = 0; i < 2; ++i) {
        glGenVertexArrays(1, &fluid->particle_vaos[i]);
        glBindVertexArray(fluid->particle_vaos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, fluid->particle_buffers[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuFluidParticle),
                              (const void *)offsetof(GpuFluidParticle, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuFluidParticle),
                              (const void *)offsetof(GpuFluidParticle, velocity));
    }
    glBindVertexArray(0);
    fluid->initialized = true;
    printf("GPU FLIP %s: %zu particles, grid: %dx%dx%d, pressure: %d\n",
           gpu_fluid_quality_name(fluid), fluid->particle_count,
           fluid->grid_x, fluid->grid_y, fluid->grid_z,
           fluid->pressure_iterations);
    return true;
}

void gpu_fluid_reset(GpuFluid *fluid) {
    size_t particle_count = 0;
    GpuFluidParticle *particles = make_initial_particles(fluid, &particle_count);
    if (!particles) return;
    fluid->particle_count = particle_count;
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, fluid->particle_buffers[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     (GLsizeiptr)(sizeof(*particles) * fluid->max_particles),
                     particles, GL_DYNAMIC_DRAW);
    }
    free(particles);
    fluid->current_particles = 0;
    fluid->impulse_count = 0;
}

static void bind_common_grid(const GpuFluid *fluid) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, fluid->grid_accum);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, fluid->grid_velocity);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, fluid->grid_velocity_old);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, fluid->divergence);
}

static void queue_impulse(GpuFluid *fluid, Vec3 center, Vec3 radius, Vec3 acceleration) {
    if (!fluid->initialized || fluid->impulse_count >= GPU_FLUID_MAX_IMPULSES) return;
    fluid->impulses[fluid->impulse_count++] = (GpuFluidImpulse){
        .center = center,
        .radius = radius,
        .acceleration = acceleration,
    };
}

void gpu_fluid_disturb(GpuFluid *fluid, Vec3 position, Vec3 drag_velocity) {
    float horizontal_speed = sqrtf(drag_velocity.x * drag_velocity.x +
                                   drag_velocity.z * drag_velocity.z);
    if (horizontal_speed > 8.0f) {
        float scale = 8.0f / horizontal_speed;
        drag_velocity.x *= scale;
        drag_velocity.z *= scale;
    }
    Vec3 acceleration = vec3(drag_velocity.x * 34.0f, 58.0f,
                             drag_velocity.z * 34.0f);
    queue_impulse(fluid, position, vec3(1.15f, 1.05f, 1.15f), acceleration);
}

void gpu_fluid_launch_collision(GpuFluid *fluid) {
    const Vec3 radius = {2.15f, 2.15f, 7.0f};
    queue_impulse(fluid, vec3(-6.15f, -0.55f, 0.0f), radius,
                  vec3(285.0f, 82.0f, 0.0f));
    queue_impulse(fluid, vec3(6.15f, -0.55f, 0.0f), radius,
                  vec3(-285.0f, 82.0f, 0.0f));
}

static void set_impulse_uniforms(const GpuFluid *fluid) {
    glUniform1i(glGetUniformLocation(fluid->force_program, "u_impulse_count"),
                fluid->impulse_count);
    for (int i = 0; i < fluid->impulse_count; ++i) {
        char name[48];
        const GpuFluidImpulse *impulse = &fluid->impulses[i];
        snprintf(name, sizeof(name), "u_impulse_center[%d]", i);
        glUniform3f(glGetUniformLocation(fluid->force_program, name),
                    impulse->center.x, impulse->center.y, impulse->center.z);
        snprintf(name, sizeof(name), "u_impulse_radius[%d]", i);
        glUniform3f(glGetUniformLocation(fluid->force_program, name),
                    impulse->radius.x, impulse->radius.y, impulse->radius.z);
        snprintf(name, sizeof(name), "u_impulse_acceleration[%d]", i);
        glUniform3f(glGetUniformLocation(fluid->force_program, name),
                    impulse->acceleration.x, impulse->acceleration.y,
                    impulse->acceleration.z);
    }
}

static void set_grid_uniforms(GLuint program, const GpuFluid *fluid) {
    glUniform3i(glGetUniformLocation(program, "u_grid"),
                fluid->grid_x, fluid->grid_y, fluid->grid_z);
    glUniform1i(glGetUniformLocation(program, "u_grid_cells"),
                (GLint)fluid->grid_cells);
    glUniform3f(glGetUniformLocation(program, "u_domain_min"),
                -GPU_FLUID_DOMAIN_HALF_EXTENT, -2.4f,
                -GPU_FLUID_DOMAIN_HALF_EXTENT);
    glUniform1f(glGetUniformLocation(program, "u_dx"), fluid->dx);
}

static void run_substep(GpuFluid *fluid, float delta_time) {
    GLuint grid_groups = groups_for(fluid->grid_cells);
    GLuint particle_groups = groups_for(fluid->particle_count);
    GLuint input_particles = fluid->particle_buffers[fluid->current_particles];
    GLuint output_particles = fluid->particle_buffers[1 - fluid->current_particles];
    bind_common_grid(fluid);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, input_particles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, output_particles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, fluid->pressure[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, fluid->pressure[1]);

    glUseProgram(fluid->clear_program);
    set_grid_uniforms(fluid->clear_program, fluid);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(fluid->p2g_program);
    set_grid_uniforms(fluid->p2g_program, fluid);
    glUniform1i(glGetUniformLocation(fluid->p2g_program, "u_particle_count"),
                (GLint)fluid->particle_count);
    glDispatchCompute(particle_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(fluid->normalize_program);
    set_grid_uniforms(fluid->normalize_program, fluid);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(fluid->force_program);
    set_grid_uniforms(fluid->force_program, fluid);
    glUniform1f(glGetUniformLocation(fluid->force_program, "u_dt"), delta_time);
    set_impulse_uniforms(fluid);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(fluid->divergence_program);
    set_grid_uniforms(fluid->divergence_program, fluid);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    int read_pressure = 0;
    int write_pressure = 1;
    glUseProgram(fluid->pressure_program);
    set_grid_uniforms(fluid->pressure_program, fluid);
    glUniform1f(glGetUniformLocation(fluid->pressure_program, "u_dt"), delta_time);
    for (int iteration = 0; iteration < fluid->pressure_iterations; ++iteration) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, fluid->pressure[read_pressure]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, fluid->pressure[write_pressure]);
        glDispatchCompute(grid_groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        int temporary = read_pressure;
        read_pressure = write_pressure;
        write_pressure = temporary;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, fluid->pressure[read_pressure]);
    glUseProgram(fluid->project_program);
    set_grid_uniforms(fluid->project_program, fluid);
    glUniform1f(glGetUniformLocation(fluid->project_program, "u_dt"), delta_time);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(fluid->g2p_program);
    set_grid_uniforms(fluid->g2p_program, fluid);
    glUniform1f(glGetUniformLocation(fluid->g2p_program, "u_dt"), delta_time);
    glUniform1i(glGetUniformLocation(fluid->g2p_program, "u_particle_count"),
                (GLint)fluid->particle_count);
    glDispatchCompute(particle_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    fluid->current_particles = 1 - fluid->current_particles;
}

void gpu_fluid_step(GpuFluid *fluid, float frame_delta) {
    if (!fluid->initialized) return;
    float remaining = fminf(frame_delta, 1.0f / 30.0f);
    int steps = 0;
    int maximum_steps = fluid->quality == GPU_FLUID_HIGH_QUALITY ? 12 : 8;
    while (remaining > 0.00001f && steps < maximum_steps) {
        float step = fminf(remaining, fluid->maximum_step);
        run_substep(fluid, step);
        remaining -= step;
        ++steps;
    }
    fluid->impulse_count = 0;
}

void gpu_fluid_draw(const GpuFluid *fluid, const Camera *camera, int width, int height) {
    if (!fluid->initialized || width <= 0 || height <= 0) return;
    Mat4 view_projection = camera_view_projection(camera, (float)width / (float)height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(fluid->render_program);
    glUniformMatrix4fv(glGetUniformLocation(fluid->render_program, "u_view_projection"),
                       1, GL_FALSE, view_projection.m);
    glUniform3f(glGetUniformLocation(fluid->render_program, "u_camera"),
                camera->position.x, camera->position.y, camera->position.z);
    glUniform1f(glGetUniformLocation(fluid->render_program, "u_point_scale"),
                (float)height * 0.175f);
    glBindVertexArray(fluid->particle_vaos[fluid->current_particles]);
    glDrawArrays(GL_POINTS, 0, (GLsizei)fluid->particle_count);
}

void gpu_fluid_draw_surface(GpuFluid *fluid, const Camera *camera, int width, int height,
                            float time_seconds, Vec3 light_direction) {
    if (!fluid->initialized || fluid->quality != GPU_FLUID_HIGH_QUALITY) return;
    gpu_surface_draw(&fluid->surface, fluid->particle_vaos[fluid->current_particles],
                     fluid->particle_count, fluid->particle_radius, camera,
                     width, height, time_seconds, light_direction);
}

const char *gpu_fluid_quality_name(const GpuFluid *fluid) {
    return fluid->quality == GPU_FLUID_HIGH_QUALITY ? "High Quality" : "Realtime";
}

void gpu_fluid_destroy(GpuFluid *fluid) {
    gpu_surface_destroy(&fluid->surface);
    GLuint programs[] = {
        fluid->clear_program, fluid->p2g_program, fluid->normalize_program,
        fluid->force_program, fluid->divergence_program, fluid->pressure_program,
        fluid->project_program, fluid->g2p_program, fluid->render_program,
    };
    for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); ++i)
        if (programs[i]) glDeleteProgram(programs[i]);
    GLuint buffers[] = {
        fluid->particle_buffers[0], fluid->particle_buffers[1], fluid->grid_accum,
        fluid->grid_velocity, fluid->grid_velocity_old, fluid->divergence,
        fluid->pressure[0], fluid->pressure[1],
    };
    glDeleteBuffers((GLsizei)(sizeof(buffers) / sizeof(buffers[0])), buffers);
    glDeleteVertexArrays(2, fluid->particle_vaos);
    memset(fluid, 0, sizeof(*fluid));
}
