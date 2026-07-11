#define WIN32_LEAN_AND_MEAN

#include <GL/gl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "../modern_gl_compat.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 720
#define MAX_BULLETS 96
#define MAX_ENEMIES 48
#define MAX_PARTICLES 220
#define PI 3.14159265358979323846f

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    bool active;
    Vec2 pos;
    Vec2 vel;
    float radius;
    float life;
} Bullet;

typedef struct {
    bool active;
    Vec2 pos;
    Vec2 vel;
    float radius;
    float speed;
    float spin;
} Enemy;

typedef struct {
    bool active;
    Vec2 pos;
    Vec2 vel;
    float life;
    float max_life;
    float size;
    float r;
    float g;
    float b;
} Particle;

static HWND window_handle;
static HDC device_context;
static HGLRC render_context;
static bool running = true;
static bool keys[256];
static bool mouse_down = false;
static Vec2 mouse_pos = {WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT * 0.5f};

static Vec2 player_pos = {WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT - 90.0f};
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static Particle particles[MAX_PARTICLES];
static int score = 0;
static int health = 5;
static bool game_over = false;
static float fire_cooldown = 0.0f;
static float spawn_timer = 0.5f;
static float title_timer = 0.0f;

static float random_float(float min, float max)
{
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

static Vec2 vec2(float x, float y)
{
    Vec2 v = {x, y};
    return v;
}

static Vec2 vec_add(Vec2 a, Vec2 b)
{
    return vec2(a.x + b.x, a.y + b.y);
}

static Vec2 vec_sub(Vec2 a, Vec2 b)
{
    return vec2(a.x - b.x, a.y - b.y);
}

static Vec2 vec_scale(Vec2 v, float scale)
{
    return vec2(v.x * scale, v.y * scale);
}

static float vec_length(Vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

static Vec2 vec_normalize(Vec2 v)
{
    const float length = vec_length(v);
    if (length < 0.0001f) {
        return vec2(0.0f, -1.0f);
    }
    return vec_scale(v, 1.0f / length);
}

static float distance_between(Vec2 a, Vec2 b)
{
    return vec_length(vec_sub(a, b));
}

static bool key_is_down(int key)
{
    return keys[key] || (GetAsyncKeyState(key) & 0x8000) != 0;
}

static void reset_game(void)
{
    player_pos = vec2(WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT - 90.0f);
    for (int i = 0; i < MAX_BULLETS; ++i) {
        bullets[i].active = false;
    }
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        enemies[i].active = false;
    }
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].active = false;
    }
    score = 0;
    health = 5;
    game_over = false;
    fire_cooldown = 0.0f;
    spawn_timer = 0.5f;
}

static void spawn_particles(Vec2 pos, float r, float g, float b, int count)
{
    for (int n = 0; n < count; ++n) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (!particles[i].active) {
                const float angle = random_float(0.0f, PI * 2.0f);
                const float speed = random_float(60.0f, 260.0f);
                particles[i].active = true;
                particles[i].pos = pos;
                particles[i].vel = vec2(cosf(angle) * speed, sinf(angle) * speed);
                particles[i].life = random_float(0.25f, 0.65f);
                particles[i].max_life = particles[i].life;
                particles[i].size = random_float(2.0f, 5.0f);
                particles[i].r = r;
                particles[i].g = g;
                particles[i].b = b;
                break;
            }
        }
    }
}

static void spawn_bullet(void)
{
    if (fire_cooldown > 0.0f || game_over) {
        return;
    }

    const Vec2 direction = vec_normalize(vec_sub(mouse_pos, player_pos));
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].pos = vec_add(player_pos, vec_scale(direction, 28.0f));
            bullets[i].vel = vec_scale(direction, 780.0f);
            bullets[i].radius = 5.0f;
            bullets[i].life = 1.35f;
            fire_cooldown = 0.10f;
            return;
        }
    }
}

static void spawn_enemy(void)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            const float radius = random_float(15.0f, 27.0f);
            const float speed = random_float(60.0f, 105.0f) + score * 0.03f;
            enemies[i].active = true;
            enemies[i].pos = vec2(random_float(radius, WINDOW_WIDTH - radius), -radius);
            enemies[i].vel = vec2(0.0f, speed);
            enemies[i].radius = radius;
            enemies[i].speed = speed;
            enemies[i].spin = random_float(0.0f, PI * 2.0f);
            return;
        }
    }
}

static void update_title(float dt)
{
    title_timer -= dt;
    if (title_timer > 0.0f) {
        return;
    }

    char title[160];
    if (game_over) {
        snprintf(title, sizeof(title), "OpenGL Shooter - GAME OVER  Score: %d  Press R to restart", score);
    } else {
        snprintf(title, sizeof(title), "OpenGL Shooter - Score: %d  HP: %d  WASD/Mouse/Space  R Reset  Esc Quit", score, health);
    }
    SetWindowTextA(window_handle, title);
    title_timer = 0.12f;
}

static void update_game(float dt)
{
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    update_title(dt);

    if (game_over) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (particles[i].active) {
                particles[i].life -= dt;
                particles[i].pos = vec_add(particles[i].pos, vec_scale(particles[i].vel, dt));
                particles[i].vel = vec_scale(particles[i].vel, 0.98f);
                if (particles[i].life <= 0.0f) {
                    particles[i].active = false;
                }
            }
        }
        return;
    }

    Vec2 movement = vec2(0.0f, 0.0f);
    if (key_is_down('A') || key_is_down(VK_LEFT)) {
        movement.x -= 1.0f;
    }
    if (key_is_down('D') || key_is_down(VK_RIGHT)) {
        movement.x += 1.0f;
    }
    if (key_is_down('W') || key_is_down(VK_UP)) {
        movement.y -= 1.0f;
    }
    if (key_is_down('S') || key_is_down(VK_DOWN)) {
        movement.y += 1.0f;
    }

    if (vec_length(movement) > 0.0f) {
        player_pos = vec_add(player_pos, vec_scale(vec_normalize(movement), 360.0f * dt));
        if (player_pos.x < 30.0f) {
            player_pos.x = 30.0f;
        }
        if (player_pos.x > WINDOW_WIDTH - 30.0f) {
            player_pos.x = WINDOW_WIDTH - 30.0f;
        }
        if (player_pos.y < 40.0f) {
            player_pos.y = 40.0f;
        }
        if (player_pos.y > WINDOW_HEIGHT - 40.0f) {
            player_pos.y = WINDOW_HEIGHT - 40.0f;
        }
    }

    fire_cooldown -= dt;
    if (mouse_down || key_is_down(VK_LBUTTON) || key_is_down(VK_SPACE)) {
        spawn_bullet();
    }

    spawn_timer -= dt;
    if (spawn_timer <= 0.0f) {
        spawn_enemy();
        const float interval = fmaxf(0.18f, 0.75f - score * 0.0015f);
        spawn_timer = random_float(interval * 0.45f, interval);
    }

    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (bullets[i].active) {
            bullets[i].life -= dt;
            bullets[i].pos = vec_add(bullets[i].pos, vec_scale(bullets[i].vel, dt));
            if (bullets[i].life <= 0.0f
                || bullets[i].pos.x < -40.0f
                || bullets[i].pos.x > WINDOW_WIDTH + 40.0f
                || bullets[i].pos.y < -40.0f
                || bullets[i].pos.y > WINDOW_HEIGHT + 40.0f) {
                bullets[i].active = false;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            const Vec2 chase = vec_normalize(vec_sub(player_pos, enemies[i].pos));
            enemies[i].vel = vec_add(vec_scale(enemies[i].vel, 0.95f), vec_scale(chase, enemies[i].speed * 0.05f));
            enemies[i].pos = vec_add(enemies[i].pos, vec_scale(enemies[i].vel, dt));
            enemies[i].spin += dt * 4.0f;

            if (enemies[i].pos.y > WINDOW_HEIGHT + enemies[i].radius) {
                enemies[i].active = false;
            }
        }
    }

    for (int e = 0; e < MAX_ENEMIES; ++e) {
        if (!enemies[e].active) {
            continue;
        }

        for (int b = 0; b < MAX_BULLETS; ++b) {
            if (!bullets[b].active) {
                continue;
            }

            if (distance_between(enemies[e].pos, bullets[b].pos) < enemies[e].radius + bullets[b].radius) {
                spawn_particles(enemies[e].pos, 1.0f, 0.45f, 0.08f, 18);
                enemies[e].active = false;
                bullets[b].active = false;
                score += 10;
                break;
            }
        }
    }

    for (int e = 0; e < MAX_ENEMIES; ++e) {
        if (enemies[e].active
            && distance_between(enemies[e].pos, player_pos) < enemies[e].radius + 20.0f) {
            spawn_particles(player_pos, 0.2f, 0.75f, 1.0f, 32);
            enemies[e].active = false;
            --health;
            if (health <= 0) {
                game_over = true;
                spawn_particles(player_pos, 1.0f, 0.25f, 0.12f, 80);
            }
        }
    }

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            particles[i].life -= dt;
            particles[i].pos = vec_add(particles[i].pos, vec_scale(particles[i].vel, dt));
            particles[i].vel = vec_scale(particles[i].vel, 0.98f);
            if (particles[i].life <= 0.0f) {
                particles[i].active = false;
            }
        }
    }
}

static void set_color(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

static void draw_circle(Vec2 center, float radius, int segments)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);
    for (int i = 0; i <= segments; ++i) {
        const float angle = (float)i / (float)segments * PI * 2.0f;
        glVertex2f(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
    }
    glEnd();
}

static void draw_ring(Vec2 center, float radius, int segments)
{
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = (float)i / (float)segments * PI * 2.0f;
        glVertex2f(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
    }
    glEnd();
}

static void draw_player(void)
{
    const Vec2 direction = vec_normalize(vec_sub(mouse_pos, player_pos));
    const Vec2 right = vec2(-direction.y, direction.x);
    const Vec2 tip = vec_add(player_pos, vec_scale(direction, 30.0f));
    const Vec2 left = vec_add(vec_add(player_pos, vec_scale(direction, -18.0f)), vec_scale(right, 18.0f));
    const Vec2 back = vec_add(player_pos, vec_scale(direction, -10.0f));
    const Vec2 right_point = vec_add(vec_add(player_pos, vec_scale(direction, -18.0f)), vec_scale(right, -18.0f));

    set_color(0.12f, 0.82f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(tip.x, tip.y);
    glVertex2f(left.x, left.y);
    glVertex2f(right_point.x, right_point.y);
    glEnd();

    set_color(0.02f, 0.18f, 0.26f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(tip.x, tip.y);
    glVertex2f(back.x, back.y);
    glVertex2f(right_point.x, right_point.y);
    glEnd();

    set_color(0.80f, 0.96f, 1.0f, 1.0f);
    draw_circle(player_pos, 7.0f, 20);
}

static void draw_enemy(const Enemy *enemy)
{
    set_color(0.95f, 0.18f, 0.12f, 1.0f);
    draw_circle(enemy->pos, enemy->radius, 28);

    set_color(1.0f, 0.64f, 0.18f, 0.75f);
    glBegin(GL_LINES);
    for (int i = 0; i < 4; ++i) {
        const float angle = enemy->spin + (float)i * PI * 0.5f;
        glVertex2f(enemy->pos.x, enemy->pos.y);
        glVertex2f(enemy->pos.x + cosf(angle) * enemy->radius, enemy->pos.y + sinf(angle) * enemy->radius);
    }
    glEnd();
}

static void draw_background(void)
{
    glLineWidth(1.0f);
    set_color(0.08f, 0.12f, 0.18f, 1.0f);
    glBegin(GL_LINES);
    for (int x = 0; x <= WINDOW_WIDTH; x += 50) {
        glVertex2f((float)x, 0.0f);
        glVertex2f((float)x, WINDOW_HEIGHT);
    }
    for (int y = 0; y <= WINDOW_HEIGHT; y += 50) {
        glVertex2f(0.0f, (float)y);
        glVertex2f(WINDOW_WIDTH, (float)y);
    }
    glEnd();
}

static void render_game(void)
{
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.02f, 0.025f, 0.045f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_background();

    glLineWidth(2.0f);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            const float alpha = particles[i].life / particles[i].max_life;
            set_color(particles[i].r, particles[i].g, particles[i].b, alpha);
            draw_circle(particles[i].pos, particles[i].size * alpha, 12);
        }
    }

    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (bullets[i].active) {
            set_color(1.0f, 0.95f, 0.42f, 1.0f);
            draw_circle(bullets[i].pos, bullets[i].radius, 16);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            draw_enemy(&enemies[i]);
        }
    }

    if (!game_over) {
        draw_player();
    } else {
        set_color(1.0f, 0.2f, 0.1f, 0.45f);
        draw_ring(player_pos, 80.0f + sinf((float)GetTickCount64() * 0.006f) * 12.0f, 72);
    }

    SwapBuffers(device_context);
}

static bool initialize_opengl(HWND hwnd)
{
    device_context = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ChoosePixelFormat(device_context, &pfd);
    if (pixel_format == 0 || !SetPixelFormat(device_context, pixel_format, &pfd)) {
        return false;
    }

    render_context = wglCreateContext(device_context);
    if (render_context == NULL || !wglMakeCurrent(device_context, render_context)) {
        return false;
    }

    if (!mgl_init()) {
        return false;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

static void shutdown_opengl(void)
{
    if (render_context != NULL) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(render_context);
        render_context = NULL;
    }
    if (device_context != NULL && window_handle != NULL) {
        ReleaseDC(window_handle, device_context);
        device_context = NULL;
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wparam < 256) {
            keys[wparam] = true;
        }
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wparam == 'R') {
            reset_game();
        }
        return 0;

    case WM_KEYUP:
        if (wparam < 256) {
            keys[wparam] = false;
        }
        return 0;

    case WM_LBUTTONDOWN:
        mouse_down = true;
        SetCapture(hwnd);
        return 0;

    case WM_LBUTTONUP:
        mouse_down = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEMOVE:
        mouse_pos.x = (float)GET_X_LPARAM(lparam);
        mouse_pos.y = (float)GET_Y_LPARAM(lparam);
        return 0;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

static double perf_seconds(void)
{
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;

    srand((unsigned int)time(NULL));

    const char class_name[] = "OpenGLShooterWindow";

    WNDCLASSA wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    window_handle = CreateWindowExA(
        0,
        class_name,
        "OpenGL Shooter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        instance,
        NULL);

    if (window_handle == NULL) {
        return 1;
    }

    if (!initialize_opengl(window_handle)) {
        MessageBoxA(window_handle, "OpenGL initialization failed.", "OpenGL Shooter", MB_ICONERROR);
        return 1;
    }

    ShowWindow(window_handle, show_command);
    UpdateWindow(window_handle);
    reset_game();

    double previous_time = perf_seconds();
    while (running) {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        const double now = perf_seconds();
        const float dt = (float)(now - previous_time);
        previous_time = now;

        update_game(dt);
        render_game();
        Sleep(1);
    }

    shutdown_opengl();
    return 0;
}
