#include "gpu_mpm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_SIZE 256u
#define MPM_DX (12.0f / (float)GPU_MPM_GRID_X)

static GLuint groups_for(size_t count) {
    return (GLuint)((count + LOCAL_SIZE - 1) / LOCAL_SIZE);
}

static float random_unit(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state >> 8) / 16777216.0f;
}

static void initialize_particle(GpuMpmParticle *particle, float x, float y, float z,
                                int material, uint32_t *random_state) {
    float jitter = 0.011f;
    particle->position[0] = x + (random_unit(random_state) - 0.5f) * jitter;
    particle->position[1] = y + (random_unit(random_state) - 0.5f) * jitter;
    particle->position[2] = z + (random_unit(random_state) - 0.5f) * jitter;
    particle->position[3] = (float)material;
    particle->velocity[0] = material == 0 ? 0.12f : material == 2 ? -0.12f : 0.0f;
    particle->velocity[1] = 0.0f;
    particle->velocity[2] = 0.0f;
    particle->velocity[3] = 0.0f;
    particle->deformation[0][0] = 1.0f;
    particle->deformation[1][1] = 1.0f;
    particle->deformation[2][2] = 1.0f;
    particle->state[0] = 1.0f;
    particle->state[1] = 0.0f;
    particle->state[2] = random_unit(random_state);
    particle->state[3] = 1.0f;
}

static GpuMpmParticle *make_initial_particles(size_t *particle_count) {
    GpuMpmParticle *particles = calloc(GPU_MPM_MAX_PARTICLES, sizeof(*particles));
    if (!particles) return NULL;
    uint32_t random_state = 0x6d2b79f5u;
    size_t count = 0;
    const float spacing = 0.064f;
    for (float z = -2.35f; z <= 2.35f && count < GPU_MPM_MAX_PARTICLES; z += spacing) {
        for (float y = 1.15f; y <= 6.65f && count < GPU_MPM_MAX_PARTICLES; y += spacing) {
            for (float x = -4.45f; x <= 3.85f && count < GPU_MPM_MAX_PARTICLES; x += spacing) {
                Vec3 elastic_delta = vec3(x + 3.0f, y - 4.65f, z - 0.30f);
                bool elastic = vec3_dot(elastic_delta, elastic_delta) < 1.42f * 1.42f;

                float clay_x = x + 0.15f;
                float clay_z = z - 0.65f;
                float clay_radius = sqrtf(clay_x * clay_x + clay_z * clay_z);
                float clay_top = 6.42f + 0.20f * sinf(clay_z * 2.4f);
                bool clay = clay_radius < 1.35f && y > 1.25f && y < clay_top;

                float granular_shape = powf(fabsf((x - 2.65f) / 1.10f), 8.0f) +
                                         powf(fabsf((z + 0.45f) / 1.75f), 8.0f);
                bool granular = granular_shape < 1.0f && y > 1.20f &&
                                y < 6.05f + 0.16f * cosf((z + 0.45f) * 1.7f);

                int material = elastic ? 0 : clay ? 1 : granular ? 2 : -1;
                if (material >= 0) {
                    initialize_particle(&particles[count++], x, y, z,
                                        material, &random_state);
                }
            }
        }
    }
    *particle_count = count;
    return particles;
}

static bool load_programs(GpuMpm *mpm) {
    mpm->clear_program = gl_compute_program_from_file("shaders/mpm_clear.comp");
    mpm->p2g_program = gl_compute_program_from_file("shaders/mpm_p2g.comp");
    mpm->grid_program = gl_compute_program_from_file("shaders/mpm_grid.comp");
    mpm->g2p_program = gl_compute_program_from_file("shaders/mpm_g2p.comp");
    mpm->particle_program = gl_program_from_files(
        "shaders/mpm_particle.vert", "shaders/mpm_particle.frag");
    mpm->shadow_program = gl_program_from_files(
        "shaders/mpm_shadow.vert", "shaders/mpm_shadow.frag");
    return mpm->clear_program && mpm->p2g_program && mpm->grid_program &&
           mpm->g2p_program && mpm->particle_program && mpm->shadow_program;
}

static void allocate_buffer(GLuint *buffer, GLsizeiptr size, const void *data) {
    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

bool gpu_mpm_init(GpuMpm *mpm) {
    memset(mpm, 0, sizeof(*mpm));
    if (!load_programs(mpm)) {
        gpu_mpm_destroy(mpm);
        return false;
    }
    size_t particle_count = 0;
    GpuMpmParticle *particles = make_initial_particles(&particle_count);
    if (!particles) {
        gpu_mpm_destroy(mpm);
        return false;
    }
    mpm->particle_count = particle_count;
    allocate_buffer(&mpm->particle_buffer,
                    (GLsizeiptr)(sizeof(*particles) * GPU_MPM_MAX_PARTICLES), particles);
    free(particles);
    allocate_buffer(&mpm->grid_accum,
                    (GLsizeiptr)(sizeof(int32_t) * 4 * GPU_MPM_GRID_CELLS), NULL);
    allocate_buffer(&mpm->grid_velocity,
                    (GLsizeiptr)(sizeof(float) * 4 * GPU_MPM_GRID_CELLS), NULL);

    glGenVertexArrays(1, &mpm->particle_vao);
    glBindVertexArray(mpm->particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, mpm->particle_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuMpmParticle),
                          (const void *)offsetof(GpuMpmParticle, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuMpmParticle),
                          (const void *)offsetof(GpuMpmParticle, velocity));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GpuMpmParticle),
                          (const void *)(offsetof(GpuMpmParticle, position) + 3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GpuMpmParticle),
                          (const void *)(offsetof(GpuMpmParticle, velocity) + 3 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GpuMpmParticle),
                          (const void *)offsetof(GpuMpmParticle, state));
    glBindVertexArray(0);
    mpm->initialized = true;
    printf("GPU MPM Material Lab: %zu particles, grid: %dx%dx%d\n",
           mpm->particle_count, GPU_MPM_GRID_X, GPU_MPM_GRID_Y, GPU_MPM_GRID_Z);
    return true;
}

void gpu_mpm_reset(GpuMpm *mpm) {
    if (!mpm->initialized) return;
    size_t particle_count = 0;
    GpuMpmParticle *particles = make_initial_particles(&particle_count);
    if (!particles) return;
    mpm->particle_count = particle_count;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mpm->particle_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(sizeof(*particles) * GPU_MPM_MAX_PARTICLES),
                 particles, GL_DYNAMIC_DRAW);
    free(particles);
    mpm->simulation_time = 0.0f;
    mpm->impulse_count = 0;
}

static void queue_impulse(GpuMpm *mpm, Vec3 center, Vec3 radius, Vec3 acceleration) {
    if (!mpm->initialized || mpm->impulse_count >= GPU_MPM_MAX_IMPULSES) return;
    mpm->impulses[mpm->impulse_count++] = (GpuMpmImpulse){
        .center = center,
        .radius = radius,
        .acceleration = acceleration,
    };
}

void gpu_mpm_stir(GpuMpm *mpm, Vec3 position, Vec3 drag_velocity) {
    float speed = sqrtf(drag_velocity.x * drag_velocity.x +
                        drag_velocity.z * drag_velocity.z);
    if (speed > 9.0f) {
        float scale = 9.0f / speed;
        drag_velocity.x *= scale;
        drag_velocity.z *= scale;
    }
    queue_impulse(mpm, position, vec3(1.35f, 1.65f, 1.35f),
                  vec3(drag_velocity.x * 42.0f, 34.0f,
                       drag_velocity.z * 42.0f));
}

void gpu_mpm_impact(GpuMpm *mpm) {
    queue_impulse(mpm, vec3(0.0f, 3.65f, 0.0f), vec3(3.4f, 3.0f, 2.8f),
                  vec3(0.0f, -720.0f, 0.0f));
    queue_impulse(mpm, vec3(0.0f, 1.2f, 0.0f), vec3(2.4f, 2.0f, 2.4f),
                  vec3(0.0f, -140.0f, 0.0f));
}

static void bind_buffers(const GpuMpm *mpm) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mpm->particle_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mpm->grid_accum);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mpm->grid_velocity);
}

static void set_common_uniforms(GLuint program, const GpuMpm *mpm, float delta_time) {
    glUniform1i(glGetUniformLocation(program, "u_particle_count"),
                (GLint)mpm->particle_count);
    glUniform1i(glGetUniformLocation(program, "u_grid_cells"), GPU_MPM_GRID_CELLS);
    glUniform3i(glGetUniformLocation(program, "u_grid"),
                GPU_MPM_GRID_X, GPU_MPM_GRID_Y, GPU_MPM_GRID_Z);
    glUniform3f(glGetUniformLocation(program, "u_domain_min"), -6.0f, -2.5f, -5.0f);
    glUniform1f(glGetUniformLocation(program, "u_dx"), MPM_DX);
    glUniform1f(glGetUniformLocation(program, "u_dt"), delta_time);
    glUniform1f(glGetUniformLocation(program, "u_time"), mpm->simulation_time);
}

static void set_impulse_uniforms(const GpuMpm *mpm) {
    glUniform1i(glGetUniformLocation(mpm->grid_program, "u_impulse_count"),
                mpm->impulse_count);
    for (int i = 0; i < mpm->impulse_count; ++i) {
        char name[48];
        const GpuMpmImpulse *impulse = &mpm->impulses[i];
        snprintf(name, sizeof(name), "u_impulse_center[%d]", i);
        glUniform3f(glGetUniformLocation(mpm->grid_program, name),
                    impulse->center.x, impulse->center.y, impulse->center.z);
        snprintf(name, sizeof(name), "u_impulse_radius[%d]", i);
        glUniform3f(glGetUniformLocation(mpm->grid_program, name),
                    impulse->radius.x, impulse->radius.y, impulse->radius.z);
        snprintf(name, sizeof(name), "u_impulse_acceleration[%d]", i);
        glUniform3f(glGetUniformLocation(mpm->grid_program, name),
                    impulse->acceleration.x, impulse->acceleration.y,
                    impulse->acceleration.z);
    }
}

static void run_substep(GpuMpm *mpm, float delta_time) {
    GLuint grid_groups = groups_for(GPU_MPM_GRID_CELLS);
    GLuint particle_groups = groups_for(mpm->particle_count);
    bind_buffers(mpm);

    glUseProgram(mpm->clear_program);
    set_common_uniforms(mpm->clear_program, mpm, delta_time);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(mpm->p2g_program);
    set_common_uniforms(mpm->p2g_program, mpm, delta_time);
    glDispatchCompute(particle_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(mpm->grid_program);
    set_common_uniforms(mpm->grid_program, mpm, delta_time);
    set_impulse_uniforms(mpm);
    glDispatchCompute(grid_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(mpm->g2p_program);
    set_common_uniforms(mpm->g2p_program, mpm, delta_time);
    glDispatchCompute(particle_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    mpm->simulation_time += delta_time;
}

void gpu_mpm_step(GpuMpm *mpm, float frame_delta) {
    if (!mpm->initialized) return;
    float remaining = fminf(frame_delta, 1.0f / 20.0f);
    const float maximum_step = 1.0f / 360.0f;
    int steps = 0;
    while (remaining > 0.00001f && steps < 18) {
        float step = fminf(remaining, maximum_step);
        run_substep(mpm, step);
        remaining -= step;
        ++steps;
    }
    mpm->impulse_count = 0;
}

void gpu_mpm_draw_shadows(const GpuMpm *mpm, const Camera *camera,
                          int width, int height) {
    if (!mpm->initialized || width <= 0 || height <= 0) return;
    Mat4 view_projection = camera_view_projection(camera, (float)width / (float)height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(mpm->shadow_program);
    glUniformMatrix4fv(glGetUniformLocation(mpm->shadow_program, "u_view_projection"),
                       1, GL_FALSE, view_projection.m);
    glUniform3f(glGetUniformLocation(mpm->shadow_program, "u_light_direction"),
                -0.32f, -0.82f, -0.46f);
    glUniform1f(glGetUniformLocation(mpm->shadow_program, "u_floor"), GPU_MPM_FLOOR_Y);
    glUniform1f(glGetUniformLocation(mpm->shadow_program, "u_point_scale"),
                (float)height * 0.11f);
    glBindVertexArray(mpm->particle_vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mpm->particle_count);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void gpu_mpm_draw_particles(const GpuMpm *mpm, const Camera *camera,
                            int width, int height) {
    if (!mpm->initialized || width <= 0 || height <= 0) return;
    float aspect = (float)width / (float)height;
    Mat4 view = camera_view_matrix(camera);
    Mat4 projection = camera_projection_matrix(camera, aspect);
    Vec3 forward = camera_forward(camera);
    Vec3 right = vec3_normalize(vec3_cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    Vec3 up = vec3_normalize(vec3_cross(right, forward));
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(mpm->particle_program);
    glUniformMatrix4fv(glGetUniformLocation(mpm->particle_program, "u_view"),
                       1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(mpm->particle_program, "u_projection"),
                       1, GL_FALSE, projection.m);
    glUniform3f(glGetUniformLocation(mpm->particle_program, "u_forward"),
                forward.x, forward.y, forward.z);
    glUniform3f(glGetUniformLocation(mpm->particle_program, "u_right"),
                right.x, right.y, right.z);
    glUniform3f(glGetUniformLocation(mpm->particle_program, "u_up"),
                up.x, up.y, up.z);
    glUniform3f(glGetUniformLocation(mpm->particle_program, "u_light_direction"),
                -0.32f, -0.82f, -0.46f);
    glUniform1f(glGetUniformLocation(mpm->particle_program, "u_point_scale"),
                (float)height * projection.m[5]);
    glUniform1f(glGetUniformLocation(mpm->particle_program, "u_particle_radius"), 0.059f);
    glBindVertexArray(mpm->particle_vao);
    glDrawArrays(GL_POINTS, 0, (GLsizei)mpm->particle_count);
}

float gpu_mpm_time(const GpuMpm *mpm) {
    return mpm->simulation_time;
}

void gpu_mpm_destroy(GpuMpm *mpm) {
    GLuint programs[] = {mpm->clear_program, mpm->p2g_program, mpm->grid_program,
                         mpm->g2p_program, mpm->particle_program, mpm->shadow_program};
    for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); ++i) {
        if (programs[i]) glDeleteProgram(programs[i]);
    }
    GLuint buffers[] = {mpm->particle_buffer, mpm->grid_accum, mpm->grid_velocity};
    glDeleteBuffers((GLsizei)(sizeof(buffers) / sizeof(buffers[0])), buffers);
    if (mpm->particle_vao) glDeleteVertexArrays(1, &mpm->particle_vao);
    memset(mpm, 0, sizeof(*mpm));
}
