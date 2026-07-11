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

#define WINDOW_WIDTH 1100
#define WINDOW_HEIGHT 760
#define MAX_TRACERS 48
#define MAX_ENEMIES 70
#define MAX_PARTICLES 520
#define ARENA_HALF 72.0f
#define PLAYER_RADIUS 0.68f
#define START_X 0.0f
#define START_Z 50.0f
#define PI 3.14159265358979323846f

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    bool active;
    Vec3 start;
    Vec3 end;
    float life;
    float max_life;
} Tracer;

typedef struct {
    bool active;
    Vec3 pos;
    float radius;
    float speed;
    float phase;
} Enemy;

typedef struct {
    bool active;
    Vec3 pos;
    Vec3 vel;
    float life;
    float max_life;
    float size;
    float r;
    float g;
    float b;
} Particle;

typedef struct {
    float x;
    float z;
    float sx;
    float sz;
    float height;
    float r;
    float g;
    float b;
} MapBlock;

static HWND window_handle;
static HDC device_context;
static HGLRC render_context;
static bool running = true;
static bool keys[256];
static bool mouse_down = false;
static bool mouse_locked = false;
static int client_width = WINDOW_WIDTH;
static int client_height = WINDOW_HEIGHT;

static Vec3 player_pos = {START_X, 1.65f, START_Z};
static float yaw = 0.0f;
static float pitch = 0.0f;
static Tracer tracers[MAX_TRACERS];
static Enemy enemies[MAX_ENEMIES];
static Particle particles[MAX_PARTICLES];
static int score = 0;
static int health = 5;
static bool game_over = false;
static float fire_cooldown = 0.0f;
static float spawn_timer = 0.4f;
static float title_timer = 0.0f;
static float muzzle_flash_timer = 0.0f;

static const MapBlock map_blocks[] = {
    {0.0f, -34.0f, 34.0f, 3.5f, 4.8f, 0.52f, 0.57f, 0.62f},
    {0.0f, 16.0f, 38.0f, 3.5f, 4.8f, 0.52f, 0.57f, 0.62f},
    {-24.0f, -10.0f, 3.5f, 42.0f, 4.8f, 0.50f, 0.55f, 0.60f},
    {24.0f, -10.0f, 3.5f, 42.0f, 4.8f, 0.50f, 0.55f, 0.60f},
    {0.0f, -8.0f, 9.0f, 9.0f, 8.0f, 0.40f, 0.46f, 0.52f},
    {-44.0f, -44.0f, 12.0f, 12.0f, 6.0f, 0.34f, 0.46f, 0.58f},
    {44.0f, -44.0f, 12.0f, 12.0f, 6.0f, 0.58f, 0.44f, 0.32f},
    {-44.0f, 38.0f, 12.0f, 12.0f, 6.0f, 0.34f, 0.46f, 0.58f},
    {44.0f, 38.0f, 12.0f, 12.0f, 6.0f, 0.58f, 0.44f, 0.32f},
    {-52.0f, -4.0f, 4.0f, 18.0f, 5.0f, 0.48f, 0.55f, 0.62f},
    {52.0f, -4.0f, 4.0f, 18.0f, 5.0f, 0.48f, 0.55f, 0.62f},
    {-12.0f, 44.0f, 18.0f, 4.0f, 4.0f, 0.36f, 0.50f, 0.46f},
    {20.0f, 48.0f, 20.0f, 4.0f, 4.0f, 0.36f, 0.50f, 0.46f},
    {-58.0f, 23.0f, 10.0f, 4.0f, 3.6f, 0.62f, 0.52f, 0.30f},
    {58.0f, 23.0f, 10.0f, 4.0f, 3.6f, 0.62f, 0.52f, 0.30f},
    {-36.0f, 8.0f, 7.0f, 7.0f, 3.4f, 0.56f, 0.42f, 0.34f},
    {36.0f, 8.0f, 7.0f, 7.0f, 3.4f, 0.56f, 0.42f, 0.34f},
    {-14.0f, -54.0f, 6.0f, 6.0f, 3.4f, 0.56f, 0.42f, 0.34f},
    {14.0f, -54.0f, 6.0f, 6.0f, 3.4f, 0.56f, 0.42f, 0.34f},
    {-32.0f, 58.0f, 5.0f, 18.0f, 3.2f, 0.30f, 0.44f, 0.57f},
    {34.0f, 58.0f, 5.0f, 18.0f, 3.2f, 0.30f, 0.44f, 0.57f},
    {0.0f, 62.0f, 16.0f, 4.0f, 3.0f, 0.62f, 0.50f, 0.28f},
    {-62.0f, -26.0f, 4.0f, 16.0f, 3.8f, 0.33f, 0.43f, 0.54f},
    {62.0f, -26.0f, 4.0f, 16.0f, 3.8f, 0.33f, 0.43f, 0.54f},
};

static float random_float(float min, float max)
{
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

static Vec3 vec3(float x, float y, float z)
{
    Vec3 v = {x, y, z};
    return v;
}

static Vec3 vec_add(Vec3 a, Vec3 b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 vec_sub(Vec3 a, Vec3 b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 vec_scale(Vec3 v, float scale)
{
    return vec3(v.x * scale, v.y * scale, v.z * scale);
}

static float vec_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float vec_length(Vec3 v)
{
    return sqrtf(vec_dot(v, v));
}

static Vec3 vec_normalize(Vec3 v)
{
    const float length = vec_length(v);
    if (length < 0.0001f) {
        return vec3(0.0f, 0.0f, -1.0f);
    }
    return vec_scale(v, 1.0f / length);
}

static Vec3 camera_forward(void)
{
    const float cp = cosf(pitch);
    return vec_normalize(vec3(-sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp));
}

static Vec3 flat_forward(void)
{
    return vec_normalize(vec3(-sinf(yaw), 0.0f, -cosf(yaw)));
}

static Vec3 flat_right(void)
{
    return vec_normalize(vec3(cosf(yaw), 0.0f, -sinf(yaw)));
}

static float distance_xz(Vec3 a, Vec3 b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

static bool key_is_down(int key)
{
    return keys[key] || (GetAsyncKeyState(key) & 0x8000) != 0;
}

static float clamp_float(float value, float min, float max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static bool circle_hits_block(float x, float z, float radius, const MapBlock *block)
{
    const float half_x = block->sx * 0.5f;
    const float half_z = block->sz * 0.5f;
    const float closest_x = clamp_float(x, block->x - half_x, block->x + half_x);
    const float closest_z = clamp_float(z, block->z - half_z, block->z + half_z);
    const float dx = x - closest_x;
    const float dz = z - closest_z;
    return dx * dx + dz * dz < radius * radius;
}

static bool circle_hits_pillar_grid(float x, float z, float radius)
{
    const float pillar_radius = 0.75f;
    for (int px = -48; px <= 48; px += 24) {
        for (int pz = -56; pz <= 56; pz += 28) {
            const float dx = x - (float)px;
            const float dz = z - (float)pz;
            const float combined = radius + pillar_radius;
            if (dx * dx + dz * dz < combined * combined) {
                return true;
            }
        }
    }
    return false;
}

static bool collides_map(Vec3 pos, float radius)
{
    if (pos.x < -ARENA_HALF + radius || pos.x > ARENA_HALF - radius
        || pos.z < -ARENA_HALF + radius || pos.z > ARENA_HALF - radius) {
        return true;
    }

    for (int i = 0; i < (int)(sizeof(map_blocks) / sizeof(map_blocks[0])); ++i) {
        if (circle_hits_block(pos.x, pos.z, radius, &map_blocks[i])) {
            return true;
        }
    }

    if (circle_hits_pillar_grid(pos.x, pos.z, radius)) {
        return true;
    }

    return false;
}

static bool ray_hits_aabb(Vec3 origin, Vec3 direction, Vec3 min, Vec3 max, float max_distance, float *hit_t)
{
    float tmin = 0.0f;
    float tmax = max_distance;

    const float origin_values[3] = {origin.x, origin.y, origin.z};
    const float direction_values[3] = {direction.x, direction.y, direction.z};
    const float min_values[3] = {min.x, min.y, min.z};
    const float max_values[3] = {max.x, max.y, max.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (fabsf(direction_values[axis]) < 0.00001f) {
            if (origin_values[axis] < min_values[axis] || origin_values[axis] > max_values[axis]) {
                return false;
            }
        } else {
            float inv = 1.0f / direction_values[axis];
            float t0 = (min_values[axis] - origin_values[axis]) * inv;
            float t1 = (max_values[axis] - origin_values[axis]) * inv;
            if (t0 > t1) {
                float temp = t0;
                t0 = t1;
                t1 = temp;
            }
            if (t0 > tmin) {
                tmin = t0;
            }
            if (t1 < tmax) {
                tmax = t1;
            }
            if (tmax < tmin) {
                return false;
            }
        }
    }

    *hit_t = tmin;
    return true;
}

static bool ray_hits_sphere(Vec3 origin, Vec3 direction, Vec3 center, float radius, float max_distance, float *hit_t)
{
    Vec3 oc = vec_sub(origin, center);
    float half_b = vec_dot(oc, direction);
    float c = vec_dot(oc, oc) - radius * radius;
    float discriminant = half_b * half_b - c;
    if (discriminant < 0.0f) {
        return false;
    }

    float sqrt_d = sqrtf(discriminant);
    float t = -half_b - sqrt_d;
    if (t < 0.0f) {
        t = -half_b + sqrt_d;
    }
    if (t < 0.0f || t > max_distance) {
        return false;
    }

    *hit_t = t;
    return true;
}

static float ray_map_distance(Vec3 origin, Vec3 direction, float max_distance)
{
    float closest = max_distance;
    float t = 0.0f;

    const Vec3 wall_min[4] = {
        {-ARENA_HALF, 0.0f, -ARENA_HALF - 0.25f},
        {-ARENA_HALF, 0.0f, ARENA_HALF - 0.25f},
        {-ARENA_HALF - 0.25f, 0.0f, -ARENA_HALF},
        {ARENA_HALF - 0.25f, 0.0f, -ARENA_HALF},
    };
    const Vec3 wall_max[4] = {
        {ARENA_HALF, 4.8f, -ARENA_HALF + 0.25f},
        {ARENA_HALF, 4.8f, ARENA_HALF + 0.25f},
        {-ARENA_HALF + 0.25f, 4.8f, ARENA_HALF},
        {ARENA_HALF + 0.25f, 4.8f, ARENA_HALF},
    };

    for (int i = 0; i < 4; ++i) {
        if (ray_hits_aabb(origin, direction, wall_min[i], wall_max[i], closest, &t)) {
            closest = t;
        }
    }

    for (int i = 0; i < (int)(sizeof(map_blocks) / sizeof(map_blocks[0])); ++i) {
        const MapBlock *block = &map_blocks[i];
        Vec3 min = vec3(block->x - block->sx * 0.5f, 0.0f, block->z - block->sz * 0.5f);
        Vec3 max = vec3(block->x + block->sx * 0.5f, block->height, block->z + block->sz * 0.5f);
        if (ray_hits_aabb(origin, direction, min, max, closest, &t)) {
            closest = t;
        }
    }

    for (int px = -48; px <= 48; px += 24) {
        for (int pz = -56; pz <= 56; pz += 28) {
            Vec3 min = vec3((float)px - 0.55f, 0.0f, (float)pz - 0.55f);
            Vec3 max = vec3((float)px + 0.55f, 6.4f, (float)pz + 0.55f);
            if (ray_hits_aabb(origin, direction, min, max, closest, &t)) {
                closest = t;
            }
        }
    }

    return closest;
}

static Vec3 move_with_collision(Vec3 current, Vec3 delta, float radius)
{
    Vec3 next = current;
    Vec3 candidate = next;

    candidate.x += delta.x;
    if (!collides_map(candidate, radius)) {
        next.x = candidate.x;
    }

    candidate = next;
    candidate.z += delta.z;
    if (!collides_map(candidate, radius)) {
        next.z = candidate.z;
    }

    next.y += delta.y;
    return next;
}

static bool spawn_point_is_clear(Vec3 pos, float radius)
{
    if (collides_map(pos, radius + 0.7f)) {
        return false;
    }
    return distance_xz(pos, player_pos) > 16.0f;
}

static void center_mouse(void)
{
    RECT rect;
    POINT point;
    if (window_handle == NULL) {
        return;
    }

    GetClientRect(window_handle, &rect);
    point.x = (rect.right - rect.left) / 2;
    point.y = (rect.bottom - rect.top) / 2;
    ClientToScreen(window_handle, &point);
    SetCursorPos(point.x, point.y);
}

static void set_mouse_lock(bool locked)
{
    if (mouse_locked == locked) {
        if (locked) {
            center_mouse();
        }
        return;
    }

    mouse_locked = locked;
    ShowCursor(locked ? FALSE : TRUE);
    if (locked) {
        SetCapture(window_handle);
        center_mouse();
    } else {
        ReleaseCapture();
    }
}

static void reset_game(void)
{
    player_pos = vec3(START_X, 1.65f, START_Z);
    yaw = 0.0f;
    pitch = 0.0f;

    for (int i = 0; i < MAX_TRACERS; ++i) {
        tracers[i].active = false;
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
    spawn_timer = 0.4f;
}

static void spawn_particles(Vec3 pos, float r, float g, float b, int count)
{
    for (int n = 0; n < count; ++n) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (!particles[i].active) {
                const float angle = random_float(0.0f, PI * 2.0f);
                const float up = random_float(0.1f, 1.0f);
                const float speed = random_float(3.0f, 11.0f);
                particles[i].active = true;
                particles[i].pos = pos;
                particles[i].vel = vec3(cosf(angle) * speed, up * speed, sinf(angle) * speed);
                particles[i].life = random_float(0.35f, 0.9f);
                particles[i].max_life = particles[i].life;
                particles[i].size = random_float(0.04f, 0.14f);
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

    const float max_distance = 96.0f;
    const Vec3 forward = camera_forward();
    const Vec3 start = vec_add(player_pos, vec_add(vec_scale(forward, 0.72f), vec3(0.0f, -0.12f, 0.0f)));
    float closest = ray_map_distance(start, forward, max_distance);
    int hit_enemy = -1;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            float t = 0.0f;
            Vec3 center = vec_add(enemies[i].pos, vec3(0.0f, enemies[i].radius * 0.65f, 0.0f));
            if (ray_hits_sphere(start, forward, center, enemies[i].radius * 1.25f, closest, &t)) {
                closest = t;
                hit_enemy = i;
            }
        }
    }

    Vec3 end = vec_add(start, vec_scale(forward, closest));

    if (hit_enemy >= 0) {
        spawn_particles(enemies[hit_enemy].pos, 1.0f, 0.34f, 0.08f, 42);
        enemies[hit_enemy].active = false;
        score += 10;
    } else if (closest < max_distance) {
        spawn_particles(end, 1.0f, 0.84f, 0.25f, 10);
    }

    for (int i = 0; i < MAX_TRACERS; ++i) {
        if (!tracers[i].active) {
            tracers[i].active = true;
            tracers[i].start = start;
            tracers[i].end = end;
            tracers[i].life = 0.065f;
            tracers[i].max_life = tracers[i].life;
            break;
        }
    }

    fire_cooldown = 0.075f;
    muzzle_flash_timer = 0.045f;
}

static void spawn_enemy(void)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            Vec3 spawn = vec3(0.0f, 0.75f, 0.0f);
            bool found = false;

            for (int attempt = 0; attempt < 40; ++attempt) {
                const int side = rand() % 4;
                float x = random_float(-ARENA_HALF + 4.0f, ARENA_HALF - 4.0f);
                float z = random_float(-ARENA_HALF + 4.0f, ARENA_HALF - 4.0f);

                if (side == 0) {
                    z = -ARENA_HALF + random_float(3.5f, 10.0f);
                } else if (side == 1) {
                    z = ARENA_HALF - random_float(3.5f, 10.0f);
                } else if (side == 2) {
                    x = -ARENA_HALF + random_float(3.5f, 10.0f);
                } else {
                    x = ARENA_HALF - random_float(3.5f, 10.0f);
                }

                spawn = vec3(x, 0.75f, z);
                if (spawn_point_is_clear(spawn, 1.1f)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                return;
            }

            enemies[i].active = true;
            enemies[i].pos = spawn;
            enemies[i].radius = random_float(0.55f, 0.9f);
            enemies[i].speed = random_float(2.0f, 3.8f) + score * 0.006f;
            enemies[i].phase = random_float(0.0f, PI * 2.0f);
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

    char title[180];
    if (game_over) {
        snprintf(title, sizeof(title), "OpenGL Shooter 3D - GAME OVER  Score: %d  R Restart  Esc Quit", score);
    } else {
        snprintf(title,
                 sizeof(title),
                 "OpenGL Shooter 3D - Pos %.1f %.1f  Score: %d  HP: %d  WASD/Arrows Move  Mouse Look",
                 player_pos.x,
                 player_pos.z,
                 score,
                 health);
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

    if (muzzle_flash_timer > 0.0f) {
        muzzle_flash_timer -= dt;
    }

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            particles[i].life -= dt;
            particles[i].vel.y -= 9.0f * dt;
            particles[i].pos = vec_add(particles[i].pos, vec_scale(particles[i].vel, dt));
            if (particles[i].pos.y < 0.04f) {
                particles[i].pos.y = 0.04f;
                particles[i].vel.y *= -0.25f;
            }
            if (particles[i].life <= 0.0f) {
                particles[i].active = false;
            }
        }
    }

    if (game_over) {
        return;
    }

    Vec3 movement = vec3(0.0f, 0.0f, 0.0f);
    if (key_is_down('W') || key_is_down(VK_UP)) {
        movement = vec_add(movement, flat_forward());
    }
    if (key_is_down('S') || key_is_down(VK_DOWN)) {
        movement = vec_sub(movement, flat_forward());
    }
    if (key_is_down('D') || key_is_down(VK_RIGHT)) {
        movement = vec_add(movement, flat_right());
    }
    if (key_is_down('A') || key_is_down(VK_LEFT)) {
        movement = vec_sub(movement, flat_right());
    }

    if (vec_length(movement) > 0.001f) {
        const float speed = key_is_down(VK_SHIFT) ? 26.0f : 16.0f;
        const Vec3 delta = vec_scale(vec_normalize(movement), speed * dt);
        player_pos = move_with_collision(player_pos, delta, PLAYER_RADIUS);
    }

    fire_cooldown -= dt;
    if (mouse_down || key_is_down(VK_LBUTTON) || key_is_down(VK_SPACE)) {
        spawn_bullet();
    }

    spawn_timer -= dt;
    if (spawn_timer <= 0.0f) {
        spawn_enemy();
        const float interval = fmaxf(0.22f, 0.85f - score * 0.0016f);
        spawn_timer = random_float(interval * 0.45f, interval);
    }

    for (int i = 0; i < MAX_TRACERS; ++i) {
        if (tracers[i].active) {
            tracers[i].life -= dt;
            if (tracers[i].life <= 0.0f) {
                tracers[i].active = false;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            Vec3 to_player = vec_sub(player_pos, enemies[i].pos);
            to_player.y = 0.0f;
            enemies[i].pos = move_with_collision(
                enemies[i].pos,
                vec_scale(vec_normalize(to_player), enemies[i].speed * dt),
                enemies[i].radius);
            enemies[i].phase += dt * 3.0f;
        }
    }

    for (int e = 0; e < MAX_ENEMIES; ++e) {
        if (enemies[e].active && distance_xz(enemies[e].pos, player_pos) < enemies[e].radius + 0.75f) {
            spawn_particles(player_pos, 0.1f, 0.75f, 1.0f, 50);
            enemies[e].active = false;
            --health;
            if (health <= 0) {
                game_over = true;
                spawn_particles(player_pos, 1.0f, 0.1f, 0.05f, 130);
                set_mouse_lock(false);
            }
        }
    }
}

static void perspective(float fov_y_degrees, float aspect, float near_z, float far_z)
{
    const float top = tanf(fov_y_degrees * PI / 360.0f) * near_z;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, near_z, far_z);
}

static void draw_cube(float sx, float sy, float sz)
{
    const float x = sx * 0.5f;
    const float y = sy * 0.5f;
    const float z = sz * 0.5f;

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-x, -y, z);
    glVertex3f(x, -y, z);
    glVertex3f(x, y, z);
    glVertex3f(-x, y, z);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(x, -y, -z);
    glVertex3f(-x, -y, -z);
    glVertex3f(-x, y, -z);
    glVertex3f(x, y, -z);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(x, -y, z);
    glVertex3f(x, -y, -z);
    glVertex3f(x, y, -z);
    glVertex3f(x, y, z);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-x, -y, -z);
    glVertex3f(-x, -y, z);
    glVertex3f(-x, y, z);
    glVertex3f(-x, y, -z);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-x, y, z);
    glVertex3f(x, y, z);
    glVertex3f(x, y, -z);
    glVertex3f(-x, y, -z);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-x, -y, -z);
    glVertex3f(x, -y, -z);
    glVertex3f(x, -y, z);
    glVertex3f(-x, -y, z);
    glEnd();
}

static void draw_colored_cube(float x, float y, float z, float sx, float sy, float sz,
                              float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3f(r, g, b);
    draw_cube(sx, sy, sz);
    glPopMatrix();
}

static void draw_light_strip(float x, float z, float sx, float sz)
{
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.96f, 0.78f);
    glBegin(GL_QUADS);
    glVertex3f(x - sx * 0.5f, 5.92f, z - sz * 0.5f);
    glVertex3f(x + sx * 0.5f, 5.92f, z - sz * 0.5f);
    glVertex3f(x + sx * 0.5f, 5.92f, z + sz * 0.5f);
    glVertex3f(x - sx * 0.5f, 5.92f, z + sz * 0.5f);
    glEnd();
    glEnable(GL_LIGHTING);
}

static void draw_target_board(float x, float z, float angle)
{
    glPushMatrix();
    glTranslatef(x, 2.0f, z);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    glColor3f(0.88f, 0.88f, 0.80f);
    draw_cube(3.2f, 3.8f, 0.16f);

    glDisable(GL_LIGHTING);
    glTranslatef(0.0f, 0.0f, -0.09f);
    glColor3f(0.12f, 0.16f, 0.20f);
    glBegin(GL_QUADS);
    glVertex3f(-0.85f, -1.05f, 0.0f);
    glVertex3f(0.85f, -1.05f, 0.0f);
    glVertex3f(0.85f, 0.85f, 0.0f);
    glVertex3f(-0.85f, 0.85f, 0.0f);
    glEnd();

    glColor3f(0.86f, 0.18f, 0.10f);
    glBegin(GL_QUADS);
    glVertex3f(-0.28f, -0.25f, -0.01f);
    glVertex3f(0.28f, -0.25f, -0.01f);
    glVertex3f(0.28f, 0.31f, -0.01f);
    glVertex3f(-0.28f, 0.31f, -0.01f);
    glEnd();
    glEnable(GL_LIGHTING);

    glPopMatrix();
}

static void draw_floor(void)
{
    glDisable(GL_LIGHTING);

    glColor3f(0.42f, 0.45f, 0.47f);
    glBegin(GL_QUADS);
    glVertex3f(-ARENA_HALF, 0.0f, -ARENA_HALF);
    glVertex3f(ARENA_HALF, 0.0f, -ARENA_HALF);
    glVertex3f(ARENA_HALF, 0.0f, ARENA_HALF);
    glVertex3f(-ARENA_HALF, 0.0f, ARENA_HALF);
    glEnd();

    glColor3f(0.33f, 0.38f, 0.42f);
    glBegin(GL_QUADS);
    glVertex3f(-9.0f, 0.012f, -ARENA_HALF);
    glVertex3f(9.0f, 0.012f, -ARENA_HALF);
    glVertex3f(9.0f, 0.012f, ARENA_HALF);
    glVertex3f(-9.0f, 0.012f, ARENA_HALF);
    glVertex3f(-ARENA_HALF, 0.013f, -9.0f);
    glVertex3f(ARENA_HALF, 0.013f, -9.0f);
    glVertex3f(ARENA_HALF, 0.013f, 9.0f);
    glVertex3f(-ARENA_HALF, 0.013f, 9.0f);
    glEnd();

    glColor3f(0.52f, 0.56f, 0.60f);
    glBegin(GL_LINES);
    for (int i = -(int)ARENA_HALF; i <= (int)ARENA_HALF; i += 4) {
        glVertex3f((float)i, 0.01f, -ARENA_HALF);
        glVertex3f((float)i, 0.01f, ARENA_HALF);
        glVertex3f(-ARENA_HALF, 0.01f, (float)i);
        glVertex3f(ARENA_HALF, 0.01f, (float)i);
    }
    glEnd();

    glLineWidth(3.0f);
    glColor3f(1.0f, 0.72f, 0.16f);
    glBegin(GL_LINES);
    glVertex3f(-ARENA_HALF, 0.03f, 0.0f);
    glVertex3f(ARENA_HALF, 0.03f, 0.0f);
    glVertex3f(0.0f, 0.03f, -ARENA_HALF);
    glVertex3f(0.0f, 0.03f, ARENA_HALF);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(0.95f, 0.95f, 0.86f);
    glBegin(GL_QUADS);
    for (int x = -48; x <= 48; x += 24) {
        glVertex3f((float)x - 0.7f, 0.04f, 18.0f);
        glVertex3f((float)x + 0.7f, 0.04f, 18.0f);
        glVertex3f((float)x + 0.7f, 0.04f, 66.0f);
        glVertex3f((float)x - 0.7f, 0.04f, 66.0f);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

static void draw_map_block(const MapBlock *block)
{
    glPushMatrix();
    glTranslatef(block->x, block->height * 0.5f, block->z);
    glColor3f(block->r, block->g, block->b);
    draw_cube(block->sx, block->height, block->sz);

    glTranslatef(0.0f, block->height * 0.5f + 0.035f, 0.0f);
    glColor3f(block->r + 0.08f, block->g + 0.08f, block->b + 0.08f);
    draw_cube(block->sx * 1.02f, 0.07f, block->sz * 1.02f);
    glPopMatrix();
}

static void draw_map(void)
{
    draw_colored_cube(0.0f, 2.4f, -ARENA_HALF, ARENA_HALF * 2.0f, 4.8f, 0.5f, 0.58f, 0.61f, 0.63f);
    draw_colored_cube(0.0f, 2.4f, ARENA_HALF, ARENA_HALF * 2.0f, 4.8f, 0.5f, 0.58f, 0.61f, 0.63f);
    draw_colored_cube(-ARENA_HALF, 2.4f, 0.0f, 0.5f, 4.8f, ARENA_HALF * 2.0f, 0.58f, 0.61f, 0.63f);
    draw_colored_cube(ARENA_HALF, 2.4f, 0.0f, 0.5f, 4.8f, ARENA_HALF * 2.0f, 0.58f, 0.61f, 0.63f);

    draw_colored_cube(0.0f, 4.1f, -ARENA_HALF + 0.35f, ARENA_HALF * 2.0f, 0.55f, 0.18f, 0.08f, 0.34f, 0.72f);
    draw_colored_cube(0.0f, 4.1f, ARENA_HALF - 0.35f, ARENA_HALF * 2.0f, 0.55f, 0.18f, 0.08f, 0.34f, 0.72f);
    draw_colored_cube(-ARENA_HALF + 0.35f, 4.1f, 0.0f, 0.18f, 0.55f, ARENA_HALF * 2.0f, 0.08f, 0.34f, 0.72f);
    draw_colored_cube(ARENA_HALF - 0.35f, 4.1f, 0.0f, 0.18f, 0.55f, ARENA_HALF * 2.0f, 0.08f, 0.34f, 0.72f);

    draw_colored_cube(0.0f, 6.0f, 0.0f, ARENA_HALF * 2.0f, 0.18f, ARENA_HALF * 2.0f, 0.24f, 0.27f, 0.30f);

    for (int i = 0; i < (int)(sizeof(map_blocks) / sizeof(map_blocks[0])); ++i) {
        draw_map_block(&map_blocks[i]);
    }

    glColor3f(0.20f, 0.48f, 0.62f);
    for (int x = -48; x <= 48; x += 24) {
        for (int z = -56; z <= 56; z += 28) {
            glPushMatrix();
            glTranslatef((float)x, 3.2f, (float)z);
            draw_cube(1.1f, 6.4f, 1.1f);
            glPopMatrix();
        }
    }

    for (int z = -54; z <= 54; z += 27) {
        draw_light_strip(-36.0f, (float)z, 18.0f, 2.2f);
        draw_light_strip(0.0f, (float)z, 22.0f, 2.2f);
        draw_light_strip(36.0f, (float)z, 18.0f, 2.2f);
    }

    for (int x = -48; x <= 48; x += 24) {
        draw_target_board((float)x, -68.5f, 0.0f);
    }

    draw_target_board(-68.5f, -36.0f, 90.0f);
    draw_target_board(68.5f, -36.0f, -90.0f);
    draw_target_board(-68.5f, 32.0f, 90.0f);
    draw_target_board(68.5f, 32.0f, -90.0f);

    draw_colored_cube(-8.5f, 1.0f, 57.0f, 2.2f, 2.0f, 8.0f, 0.12f, 0.16f, 0.19f);
    draw_colored_cube(8.5f, 1.0f, 57.0f, 2.2f, 2.0f, 8.0f, 0.12f, 0.16f, 0.19f);
    draw_colored_cube(0.0f, 2.4f, 57.0f, 20.0f, 0.7f, 1.1f, 0.85f, 0.70f, 0.22f);
}

static void draw_enemy(const Enemy *enemy)
{
    glPushMatrix();
    glTranslatef(enemy->pos.x, enemy->pos.y + sinf(enemy->phase) * 0.12f, enemy->pos.z);
    glRotatef(enemy->phase * 45.0f, 0.0f, 1.0f, 0.0f);

    glColor3f(0.95f, 0.22f, 0.12f);
    draw_cube(enemy->radius * 1.4f, enemy->radius * 1.8f, enemy->radius * 1.4f);

    glTranslatef(0.0f, enemy->radius * 1.15f, 0.0f);
    glColor3f(1.0f, 0.58f, 0.18f);
    draw_cube(enemy->radius * 0.9f, enemy->radius * 0.55f, enemy->radius * 0.9f);
    glPopMatrix();
}

static void draw_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            const float alpha = particles[i].life / particles[i].max_life;
            glPushMatrix();
            glTranslatef(particles[i].pos.x, particles[i].pos.y, particles[i].pos.z);
            glColor4f(particles[i].r, particles[i].g, particles[i].b, alpha);
            draw_cube(particles[i].size, particles[i].size, particles[i].size);
            glPopMatrix();
        }
    }
}

static void draw_blob_shadow(Vec3 pos, float radius, float alpha)
{
    glDisable(GL_LIGHTING);
    glColor4f(0.02f, 0.025f, 0.03f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(pos.x, 0.045f, pos.z);
    for (int i = 0; i <= 28; ++i) {
        const float angle = (float)i / 28.0f * PI * 2.0f;
        glVertex3f(pos.x + cosf(angle) * radius, 0.045f, pos.z + sinf(angle) * radius * 0.72f);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

static void draw_contact_shadows(void)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            draw_blob_shadow(enemies[i].pos, enemies[i].radius * 1.6f, 0.28f);
        }
    }
}

static void draw_tracers(void)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    for (int i = 0; i < MAX_TRACERS; ++i) {
        if (tracers[i].active) {
            const float alpha = tracers[i].life / tracers[i].max_life;
            glColor4f(1.0f, 0.92f, 0.35f, alpha);
            glVertex3f(tracers[i].start.x, tracers[i].start.y, tracers[i].start.z);
            glColor4f(1.0f, 0.55f, 0.12f, alpha * 0.25f);
            glVertex3f(tracers[i].end.x, tracers[i].end.y, tracers[i].end.z);
        }
    }
    glEnd();

    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

static void draw_weapon(void)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, client_width, client_height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float cx = client_width * 0.5f;
    const float cy = client_height * 0.5f;

    glLineWidth(2.0f);
    glColor3f(0.15f, 0.28f, 0.34f);
    glBegin(GL_LINES);
    glVertex2f(cx - 14.0f, cy);
    glVertex2f(cx - 5.0f, cy);
    glVertex2f(cx + 5.0f, cy);
    glVertex2f(cx + 14.0f, cy);
    glVertex2f(cx, cy - 14.0f);
    glVertex2f(cx, cy - 5.0f);
    glVertex2f(cx, cy + 5.0f);
    glVertex2f(cx, cy + 14.0f);
    glEnd();

    glColor3f(0.08f, 0.09f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2f(client_width * 0.58f, client_height);
    glVertex2f(client_width * 0.77f, client_height);
    glVertex2f(client_width * 0.70f, client_height * 0.74f);
    glVertex2f(client_width * 0.61f, client_height * 0.75f);
    glEnd();

    glColor3f(0.22f, 0.24f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(client_width * 0.59f, client_height * 0.76f);
    glVertex2f(client_width * 0.77f, client_height * 0.74f);
    glVertex2f(client_width * 0.77f, client_height * 0.67f);
    glVertex2f(client_width * 0.60f, client_height * 0.67f);
    glEnd();

    glColor3f(0.05f, 0.06f, 0.07f);
    glBegin(GL_QUADS);
    glVertex2f(client_width * 0.68f, client_height * 0.69f);
    glVertex2f(client_width * 0.84f, client_height * 0.68f);
    glVertex2f(client_width * 0.84f, client_height * 0.64f);
    glVertex2f(client_width * 0.68f, client_height * 0.65f);
    glEnd();

    glColor3f(0.55f, 0.57f, 0.56f);
    glBegin(GL_QUADS);
    glVertex2f(client_width * 0.61f, client_height * 0.69f);
    glVertex2f(client_width * 0.74f, client_height * 0.68f);
    glVertex2f(client_width * 0.74f, client_height * 0.66f);
    glVertex2f(client_width * 0.61f, client_height * 0.66f);
    glEnd();

    glColor3f(0.02f, 0.03f, 0.035f);
    glBegin(GL_QUADS);
    glVertex2f(client_width * 0.64f, client_height * 0.84f);
    glVertex2f(client_width * 0.71f, client_height * 0.84f);
    glVertex2f(client_width * 0.70f, client_height * 0.77f);
    glVertex2f(client_width * 0.65f, client_height * 0.77f);
    glEnd();

    if (muzzle_flash_timer > 0.0f) {
        const float flash = muzzle_flash_timer / 0.045f;
        glColor4f(1.0f, 0.78f, 0.18f, flash);
        glBegin(GL_TRIANGLES);
        glVertex2f(client_width * 0.84f, client_height * 0.66f);
        glVertex2f(client_width * 0.93f, client_height * 0.60f);
        glVertex2f(client_width * 0.89f, client_height * 0.70f);
        glEnd();

        glColor4f(1.0f, 0.95f, 0.55f, flash);
        glBegin(GL_TRIANGLES);
        glVertex2f(client_width * 0.84f, client_height * 0.66f);
        glVertex2f(client_width * 0.90f, client_height * 0.63f);
        glVertex2f(client_width * 0.88f, client_height * 0.69f);
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

static void render_game(void)
{
    const float aspect = client_height > 0 ? (float)client_width / (float)client_height : 1.0f;
    const GLfloat light_position[] = {0.0f, 18.0f, 0.0f, 1.0f};
    const GLfloat light_color[] = {1.0f, 0.96f, 0.86f, 1.0f};
    const GLfloat fill_position[] = {-38.0f, 9.0f, 46.0f, 1.0f};
    const GLfloat fill_color[] = {0.42f, 0.56f, 0.72f, 1.0f};

    glViewport(0, 0, client_width, client_height);
    glClearColor(0.62f, 0.69f, 0.76f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(68.0f, aspect, 0.08f, 220.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-pitch * 180.0f / PI, 1.0f, 0.0f, 0.0f);
    glRotatef(-yaw * 180.0f / PI, 0.0f, 1.0f, 0.0f);
    glTranslatef(-player_pos.x, -player_pos.y, -player_pos.z);

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_color);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_color);
    glLightfv(GL_LIGHT1, GL_POSITION, fill_position);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fill_color);

    draw_floor();
    draw_contact_shadows();
    draw_map();

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            draw_enemy(&enemies[i]);
        }
    }

    draw_particles();
    draw_tracers();
    draw_weapon();

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
    pfd.cDepthBits = 24;
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

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    {
        const GLfloat ambient[] = {0.34f, 0.36f, 0.38f, 1.0f};
        const GLfloat specular[] = {0.35f, 0.35f, 0.32f, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);
    }
    glShadeModel(GL_SMOOTH);
    return true;
}

static void shutdown_opengl(void)
{
    set_mouse_lock(false);
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

    case WM_SETFOCUS:
        if (!game_over) {
            set_mouse_lock(true);
        }
        return 0;

    case WM_KILLFOCUS:
        set_mouse_lock(false);
        return 0;

    case WM_SIZE:
        client_width = LOWORD(lparam);
        client_height = HIWORD(lparam);
        if (client_width < 1) {
            client_width = 1;
        }
        if (client_height < 1) {
            client_height = 1;
        }
        return 0;

    case WM_KEYDOWN:
        if (wparam < 256) {
            keys[wparam] = true;
        }
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wparam == 'R') {
            reset_game();
            set_mouse_lock(true);
        }
        return 0;

    case WM_KEYUP:
        if (wparam < 256) {
            keys[wparam] = false;
        }
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        mouse_down = true;
        if (!mouse_locked && !game_over) {
            set_mouse_lock(true);
        }
        return 0;

    case WM_LBUTTONUP:
        mouse_down = false;
        return 0;

    case WM_MOUSEMOVE:
        if (mouse_locked && GetForegroundWindow() == hwnd) {
            const int cx = client_width / 2;
            const int cy = client_height / 2;
            const int mx = GET_X_LPARAM(lparam);
            const int my = GET_Y_LPARAM(lparam);
            const int dx = mx - cx;
            const int dy = my - cy;

            if (dx != 0 || dy != 0) {
                yaw -= (float)dx * 0.0023f;
                pitch -= (float)dy * 0.0023f;
                if (pitch > 1.25f) {
                    pitch = 1.25f;
                }
                if (pitch < -1.25f) {
                    pitch = -1.25f;
                }
                center_mouse();
            }
        }
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

    const char class_name[] = "OpenGLShooter3DWindow";

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
        "OpenGL Shooter 3D",
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
        MessageBoxA(window_handle, "OpenGL initialization failed.", "OpenGL Shooter 3D", MB_ICONERROR);
        return 1;
    }

    ShowWindow(window_handle, show_command);
    UpdateWindow(window_handle);
    SetForegroundWindow(window_handle);
    set_mouse_lock(true);
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
