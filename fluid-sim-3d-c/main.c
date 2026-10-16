#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include "camera.h"
#include "fluid_sim.h"
#include "gl_api.h"
#include "gpu_fluid.h"
#include "gpu_mpm.h"
#include "platform.h"
#include "physics_world.h"
#include "renderer.h"

#include <math.h>
#include <omp.h>
#include <stdio.h>

typedef enum {
    APP_MODE_PREVIEW,
    APP_MODE_GPU_REALTIME,
    APP_MODE_HIGH_QUALITY,
    APP_MODE_MATERIAL_LAB,
} AppMode;

static float frame_seconds(LARGE_INTEGER now, LARGE_INTEGER previous, LARGE_INTEGER frequency) {
    return (float)((double)(now.QuadPart - previous.QuadPart) / (double)frequency.QuadPart);
}

int main(void) {
    Platform platform;
    FluidSim fluid;
    Renderer renderer;
    Camera camera;
    PhysicsWorld physics;
    GpuFluid gpu_fluid;
    GpuFluid high_quality_fluid = {0};
    GpuMpm material_lab = {0};

    if (!platform_init(&platform, GetModuleHandleA(NULL), 1440, 900,
                       "fluid-sim-3d-c - nonlinear colliding waves")) {
        MessageBoxA(NULL, "Window or OpenGL context creation failed.", "fluid-sim-3d-c", MB_ICONERROR);
        platform_shutdown(&platform);
        return 1;
    }
    if (!gl_api_load() || !fluid_sim_init(&fluid)) {
        MessageBoxA(NULL, "OpenGL loading or fluid allocation failed.", "fluid-sim-3d-c", MB_ICONERROR);
        platform_shutdown(&platform);
        return 1;
    }
    if (!renderer_init(&renderer, &fluid)) {
        MessageBoxA(NULL, "Shader or renderer initialization failed. See the terminal.",
                    "fluid-sim-3d-c", MB_ICONERROR);
        fluid_sim_destroy(&fluid);
        platform_shutdown(&platform);
        return 1;
    }

    camera_reset(&camera);
    physics_world_init(&physics);
    bool gpu_fluid_available = gpu_fluid_init(&gpu_fluid, GPU_FLUID_REALTIME);
    bool high_quality_available = false;
    bool high_quality_attempted = false;
    bool material_lab_available = false;
    bool material_lab_attempted = false;
    AppMode mode = APP_MODE_PREVIEW;
    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("VSync: %s\n", platform.vsync_enabled ? "on" : "unavailable");
    printf("Fluid workers: %d, grid: %dx%d, physics: 120 Hz\n",
           omp_get_max_threads(), FLUID_RESOLUTION, FLUID_RESOLUTION);
    printf("G realtime FLIP, H high quality surface, M material lab\n");
    printf("WASD/Space/C move, RMB look, wheel zoom, LMB force, F impact, R reset, Esc exit\n");

    LARGE_INTEGER frequency;
    LARGE_INTEGER previous;
    LARGE_INTEGER start;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);
    start = previous;
    double accumulator = 0.0;
    float simulation_time = 0.0f;
    float fps_report_time = 0.0f;
    unsigned frame_count = 0;
    bool previous_f = false;
    bool previous_r = false;
    bool previous_g = false;
    bool previous_h = false;
    bool previous_m = false;
    bool gpu_drag_valid = false;
    Vec3 previous_gpu_drag = vec3(0.0f, 0.0f, 0.0f);

    while (!platform.input.quit) {
        platform_poll(&platform);
        if (platform.input.quit) break;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float delta_time = frame_seconds(now, previous, frequency);
        float time_seconds = frame_seconds(now, start, frequency);
        previous = now;
        if (delta_time > 0.08f) delta_time = 0.08f;

        bool pressed_f = platform.input.keys['F'];
        bool pressed_r = platform.input.keys['R'];
        bool pressed_g = platform.input.keys['G'];
        bool pressed_h = platform.input.keys['H'];
        bool pressed_m = platform.input.keys['M'];
        if (pressed_g && !previous_g && gpu_fluid_available) {
            mode = mode == APP_MODE_GPU_REALTIME ? APP_MODE_PREVIEW : APP_MODE_GPU_REALTIME;
            if (mode == APP_MODE_PREVIEW) camera_reset(&camera);
            else camera_reset_cinematic(&camera);
            accumulator = 0.0;
            gpu_drag_valid = false;
        }
        if (pressed_h && !previous_h) {
            if (!high_quality_attempted) {
                high_quality_attempted = true;
                high_quality_available = gpu_fluid_init(&high_quality_fluid,
                                                        GPU_FLUID_HIGH_QUALITY);
            }
            if (high_quality_available) {
                mode = mode == APP_MODE_HIGH_QUALITY ? APP_MODE_PREVIEW : APP_MODE_HIGH_QUALITY;
                if (mode == APP_MODE_PREVIEW) camera_reset(&camera);
                else camera_reset_cinematic(&camera);
                accumulator = 0.0;
                gpu_drag_valid = false;
            }
        }
        if (pressed_m && !previous_m) {
            if (!material_lab_attempted) {
                material_lab_attempted = true;
                material_lab_available = gpu_mpm_init(&material_lab);
            }
            if (material_lab_available) {
                mode = mode == APP_MODE_MATERIAL_LAB ? APP_MODE_PREVIEW : APP_MODE_MATERIAL_LAB;
                if (mode == APP_MODE_PREVIEW) camera_reset(&camera);
                else camera_reset_material_lab(&camera);
                accumulator = 0.0;
                gpu_drag_valid = false;
            }
        }
        bool fluid_gpu_mode = mode == APP_MODE_GPU_REALTIME ||
                              mode == APP_MODE_HIGH_QUALITY;
        bool material_mode = mode == APP_MODE_MATERIAL_LAB;
        GpuFluid *active_gpu = mode == APP_MODE_HIGH_QUALITY ?
                               &high_quality_fluid : &gpu_fluid;
        if (pressed_f && !previous_f) {
            if (material_mode) gpu_mpm_impact(&material_lab);
            else if (fluid_gpu_mode) gpu_fluid_launch_collision(active_gpu);
            else fluid_sim_launch_collision(&fluid, 2.55f);
        }
        if (pressed_r && !previous_r) {
            fluid_sim_reset(&fluid);
            physics_world_reset(&physics);
            if (gpu_fluid_available) gpu_fluid_reset(&gpu_fluid);
            if (high_quality_available) gpu_fluid_reset(&high_quality_fluid);
            if (material_lab_available) gpu_mpm_reset(&material_lab);
            if (material_mode) camera_reset_material_lab(&camera);
            else if (fluid_gpu_mode) camera_reset_cinematic(&camera);
            else camera_reset(&camera);
            simulation_time = 0.0f;
            accumulator = 0.0;
        }
        previous_f = pressed_f;
        previous_r = pressed_r;
        previous_g = pressed_g;
        previous_h = pressed_h;
        previous_m = pressed_m;
        camera_update(&camera, &platform.input, delta_time);
        bool object_interaction = mode != APP_MODE_PREVIEW ? true : physics_world_update_interaction(
            &physics, &platform.input, &camera, &fluid);

        if (fluid_gpu_mode) {
            Vec3 hit;
            bool over_fluid = platform.input.mouse_down &&
                camera_ray_to_horizontal_plane(&camera, &platform.input,
                    GPU_FLUID_INTERACTION_SURFACE_Y,
                    GPU_FLUID_DOMAIN_HALF_EXTENT - 0.25f, &hit);
            if (over_fluid) {
                Vec3 drag_velocity = vec3(0.0f, 0.0f, 0.0f);
                if (gpu_drag_valid && delta_time > 0.0001f) {
                    drag_velocity = vec3_scale(vec3_sub(hit, previous_gpu_drag),
                                               1.0f / delta_time);
                }
                gpu_fluid_disturb(active_gpu, hit, drag_velocity);
                previous_gpu_drag = hit;
                gpu_drag_valid = true;
            } else {
                gpu_drag_valid = false;
            }
            gpu_fluid_step(active_gpu, delta_time);
        } else if (material_mode) {
            Vec3 hit;
            bool over_material = platform.input.mouse_down &&
                camera_ray_to_horizontal_plane(&camera, &platform.input,
                                               1.15f, 5.65f, &hit);
            if (over_material) {
                Vec3 drag_velocity = vec3(0.0f, 0.0f, 0.0f);
                if (gpu_drag_valid && delta_time > 0.0001f) {
                    drag_velocity = vec3_scale(vec3_sub(hit, previous_gpu_drag),
                                               1.0f / delta_time);
                }
                gpu_mpm_stir(&material_lab, hit, drag_velocity);
                previous_gpu_drag = hit;
                gpu_drag_valid = true;
            } else {
                gpu_drag_valid = false;
            }
            gpu_mpm_step(&material_lab, delta_time);
        } else {
            gpu_drag_valid = false;
            accumulator += delta_time;
            int simulation_steps = 0;
            while (accumulator >= FLUID_FIXED_STEP && simulation_steps < 8) {
                if (platform.input.mouse_down && !object_interaction) {
                    float water_x;
                    float water_z;
                    if (camera_ray_to_water(&camera, &platform.input, FLUID_WORLD_SIZE,
                                            &water_x, &water_z)) {
                        fluid_sim_disturb(&fluid, water_x, water_z, 0.055f, 1.7f);
                    }
                }
                simulation_time += FLUID_FIXED_STEP;
                fluid_sim_step(&fluid, FLUID_FIXED_STEP, simulation_time);
                physics_world_step(&physics, &fluid, FLUID_FIXED_STEP);
                accumulator -= FLUID_FIXED_STEP;
                ++simulation_steps;
            }
            if (accumulator >= FLUID_FIXED_STEP) {
                accumulator = fmod(accumulator, FLUID_FIXED_STEP);
            }
        }

        float interpolation = (float)(accumulator / FLUID_FIXED_STEP);
        if (mode == APP_MODE_HIGH_QUALITY) {
            renderer_draw_high_quality(&renderer, &high_quality_fluid, &camera,
                                       platform.input.width, platform.input.height,
                                       time_seconds);
        } else if (mode == APP_MODE_GPU_REALTIME) {
            renderer_draw_cinematic(&renderer, &gpu_fluid, &camera,
                                    platform.input.width, platform.input.height, time_seconds);
        } else if (mode == APP_MODE_MATERIAL_LAB) {
            renderer_draw_material_lab(&renderer, &material_lab, &camera,
                                       platform.input.width, platform.input.height,
                                       time_seconds);
        } else {
            renderer_draw(&renderer, &fluid, &physics, &camera, platform.input.width,
                          platform.input.height, time_seconds, interpolation);
        }
        HudMode hud_mode = mode == APP_MODE_HIGH_QUALITY ? HUD_MODE_HIGH_QUALITY :
                           mode == APP_MODE_GPU_REALTIME ? HUD_MODE_REALTIME_FLIP :
                           mode == APP_MODE_MATERIAL_LAB ? HUD_MODE_MATERIAL_LAB :
                           HUD_MODE_PREVIEW;
        renderer_draw_mode_hud(&renderer, hud_mode,
                               platform.input.width, platform.input.height);
        platform_swap(&platform);

        ++frame_count;
        if (time_seconds - fps_report_time >= 1.0f) {
            float interval = time_seconds - fps_report_time;
            float fps = interval > 0.0f ? (float)frame_count / interval : 0.0f;
            char title[192];
            const char *mode_name = mode == APP_MODE_HIGH_QUALITY ? "High Quality Surface" :
                                    mode == APP_MODE_GPU_REALTIME ? "Realtime FLIP particles" :
                                    mode == APP_MODE_MATERIAL_LAB ? "MPM Material Lab" :
                                    "Preview";
            snprintf(title, sizeof(title),
                     "fluid-sim-3d-c | %.0f FPS | %s | %d objects | %s | G realtime | H high | M materials | VSync %s",
                     fps, mode_name,
                     physics_world_group_count(&physics),
                     physics_prefab_name(physics.selected_prefab),
                     platform.vsync_enabled ? "on" : "off");
            SetWindowTextA(platform.window, title);
            fps_report_time = time_seconds;
            frame_count = 0;
        }
    }

    renderer_destroy(&renderer);
    if (gpu_fluid_available) gpu_fluid_destroy(&gpu_fluid);
    if (high_quality_available) gpu_fluid_destroy(&high_quality_fluid);
    if (material_lab_available) gpu_mpm_destroy(&material_lab);
    fluid_sim_destroy(&fluid);
    platform_shutdown(&platform);
    return 0;
}
