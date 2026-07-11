#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define IMAGE_WIDTH 720
#define IMAGE_HEIGHT 405
#define MAX_DEPTH 4
#define EPSILON 0.001f
#define PI 3.14159265358979323846f

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

typedef struct {
    Vec3 color;
    float diffuse;
    float specular;
    float reflective;
} Material;

typedef struct {
    Vec3 center;
    float radius;
    Material material;
} Sphere;

typedef struct {
    Vec3 point;
    Vec3 normal;
    Material material;
} Plane;

typedef struct {
    bool hit;
    float t;
    Vec3 point;
    Vec3 normal;
    Material material;
} Hit;

static HWND window_handle;
static uint32_t pixels[IMAGE_WIDTH * IMAGE_HEIGHT];
static bool rendered = false;

static const Sphere spheres[] = {
    {{-1.55f, 0.75f, -4.2f}, 0.75f, {{0.95f, 0.28f, 0.18f}, 0.85f, 0.45f, 0.16f}},
    {{0.20f, 1.00f, -5.1f}, 1.00f, {{0.25f, 0.70f, 1.00f}, 0.82f, 0.70f, 0.34f}},
    {{1.95f, 0.62f, -4.0f}, 0.62f, {{0.95f, 0.78f, 0.26f}, 0.75f, 0.55f, 0.22f}},
    {{-0.90f, 0.42f, -2.85f}, 0.42f, {{0.75f, 0.95f, 0.58f}, 0.80f, 0.38f, 0.10f}},
};

static const Plane floor_plane = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {{0.82f, 0.82f, 0.78f}, 0.92f, 0.18f, 0.08f},
};

static Vec3 light_pos = {-2.4f, 5.6f, -1.2f};
static Vec3 camera_pos = {0.0f, 1.35f, 1.55f};

static Vec3 vec3(float x, float y, float z)
{
    Vec3 v = {x, y, z};
    return v;
}

static Vec3 add(Vec3 a, Vec3 b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 sub(Vec3 a, Vec3 b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 mul(Vec3 v, float s)
{
    return vec3(v.x * s, v.y * s, v.z * s);
}

static float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float length(Vec3 v)
{
    return sqrtf(dot(v, v));
}

static Vec3 normalize(Vec3 v)
{
    float len = length(v);
    if (len < 0.00001f) {
        return vec3(0.0f, 0.0f, 0.0f);
    }
    return mul(v, 1.0f / len);
}

static Vec3 reflect(Vec3 v, Vec3 n)
{
    return sub(v, mul(n, 2.0f * dot(v, n)));
}

static Vec3 clamp_color(Vec3 c)
{
    if (c.x < 0.0f) c.x = 0.0f;
    if (c.y < 0.0f) c.y = 0.0f;
    if (c.z < 0.0f) c.z = 0.0f;
    if (c.x > 1.0f) c.x = 1.0f;
    if (c.y > 1.0f) c.y = 1.0f;
    if (c.z > 1.0f) c.z = 1.0f;
    return c;
}

static uint32_t pack_bgra(Vec3 color)
{
    color = clamp_color(color);
    uint32_t r = (uint32_t)(powf(color.x, 1.0f / 2.2f) * 255.0f);
    uint32_t g = (uint32_t)(powf(color.y, 1.0f / 2.2f) * 255.0f);
    uint32_t b = (uint32_t)(powf(color.z, 1.0f / 2.2f) * 255.0f);
    return b | (g << 8) | (r << 16);
}

static bool hit_sphere(Ray ray, const Sphere *sphere, float t_min, float t_max, Hit *hit)
{
    Vec3 oc = sub(ray.origin, sphere->center);
    float a = dot(ray.direction, ray.direction);
    float half_b = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere->radius * sphere->radius;
    float discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0f) {
        return false;
    }

    float sqrt_d = sqrtf(discriminant);
    float root = (-half_b - sqrt_d) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrt_d) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    hit->hit = true;
    hit->t = root;
    hit->point = add(ray.origin, mul(ray.direction, root));
    hit->normal = normalize(sub(hit->point, sphere->center));
    hit->material = sphere->material;
    return true;
}

static bool hit_plane(Ray ray, const Plane *plane, float t_min, float t_max, Hit *hit)
{
    float denom = dot(plane->normal, ray.direction);
    if (fabsf(denom) < 0.0001f) {
        return false;
    }

    float t = dot(sub(plane->point, ray.origin), plane->normal) / denom;
    if (t < t_min || t > t_max) {
        return false;
    }

    hit->hit = true;
    hit->t = t;
    hit->point = add(ray.origin, mul(ray.direction, t));
    hit->normal = plane->normal;
    hit->material = plane->material;
    return true;
}

static bool hit_scene(Ray ray, float t_min, float t_max, Hit *best)
{
    Hit temp = {0};
    bool found = false;
    float closest = t_max;

    for (int i = 0; i < (int)(sizeof(spheres) / sizeof(spheres[0])); ++i) {
        if (hit_sphere(ray, &spheres[i], t_min, closest, &temp)) {
            found = true;
            closest = temp.t;
            *best = temp;
        }
    }

    if (hit_plane(ray, &floor_plane, t_min, closest, &temp)) {
        found = true;
        *best = temp;
    }

    return found;
}

static Vec3 sky_color(Vec3 direction)
{
    float t = 0.5f * (direction.y + 1.0f);
    Vec3 horizon = vec3(0.92f, 0.96f, 1.0f);
    Vec3 zenith = vec3(0.42f, 0.62f, 0.94f);
    return add(mul(horizon, 1.0f - t), mul(zenith, t));
}

static Vec3 floor_pattern(Vec3 point, Vec3 base)
{
    int ix = (int)floorf(point.x * 1.4f);
    int iz = (int)floorf(point.z * 1.4f);
    bool checker = ((ix + iz) & 1) != 0;
    float stripe = fabsf(fmodf(point.x + 100.0f, 2.0f) - 1.0f);

    Vec3 color = checker ? mul(base, 0.78f) : base;
    if (stripe < 0.035f) {
        color = vec3(0.95f, 0.78f, 0.28f);
    }
    return color;
}

static bool in_shadow(Vec3 point, Vec3 light_dir, float light_distance)
{
    Ray shadow_ray = {add(point, mul(light_dir, EPSILON * 8.0f)), light_dir};
    Hit shadow_hit = {0};
    return hit_scene(shadow_ray, EPSILON, light_distance - EPSILON, &shadow_hit);
}

static Vec3 trace_ray(Ray ray, int depth)
{
    if (depth <= 0) {
        return vec3(0.0f, 0.0f, 0.0f);
    }

    Hit hit = {0};
    if (!hit_scene(ray, EPSILON, 1000.0f, &hit)) {
        return sky_color(ray.direction);
    }

    Vec3 material_color = hit.material.color;
    if (fabsf(hit.normal.y - 1.0f) < 0.001f) {
        material_color = floor_pattern(hit.point, material_color);
    }

    Vec3 to_light = sub(light_pos, hit.point);
    float light_distance = length(to_light);
    Vec3 light_dir = normalize(to_light);
    Vec3 view_dir = mul(ray.direction, -1.0f);
    Vec3 half_vec = normalize(add(light_dir, view_dir));

    float diffuse = fmaxf(0.0f, dot(hit.normal, light_dir));
    float specular = powf(fmaxf(0.0f, dot(hit.normal, half_vec)), 80.0f) * hit.material.specular;
    float shadow = in_shadow(hit.point, light_dir, light_distance) ? 0.23f : 1.0f;

    Vec3 ambient = mul(material_color, 0.12f);
    Vec3 diffuse_color = mul(material_color, diffuse * hit.material.diffuse * shadow);
    Vec3 specular_color = mul(vec3(1.0f, 0.96f, 0.88f), specular * shadow);
    Vec3 color = add(add(ambient, diffuse_color), specular_color);

    if (hit.material.reflective > 0.001f) {
        Ray reflected = {
            add(hit.point, mul(hit.normal, EPSILON * 8.0f)),
            normalize(reflect(ray.direction, hit.normal)),
        };
        Vec3 reflected_color = trace_ray(reflected, depth - 1);
        color = add(mul(color, 1.0f - hit.material.reflective), mul(reflected_color, hit.material.reflective));
    }

    return color;
}

static Ray camera_ray(float u, float v)
{
    float aspect = (float)IMAGE_WIDTH / (float)IMAGE_HEIGHT;
    float fov = 55.0f * PI / 180.0f;
    float viewport_height = 2.0f * tanf(fov * 0.5f);
    float viewport_width = aspect * viewport_height;

    Vec3 forward = normalize(sub(vec3(0.0f, 0.85f, -4.2f), camera_pos));
    Vec3 right = normalize(vec3(forward.z, 0.0f, -forward.x));
    Vec3 up = normalize(vec3(-right.z * forward.y, right.x * forward.z - right.z * forward.x, right.x * forward.y));

    Vec3 horizontal = mul(right, viewport_width);
    Vec3 vertical = mul(up, viewport_height);
    Vec3 lower_left = sub(sub(add(camera_pos, forward), mul(horizontal, 0.5f)), mul(vertical, 0.5f));
    Vec3 direction = sub(add(add(lower_left, mul(horizontal, u)), mul(vertical, v)), camera_pos);

    Ray ray = {camera_pos, normalize(direction)};
    return ray;
}

static void render_image(void)
{
    const float offsets[4][2] = {
        {0.25f, 0.25f},
        {0.75f, 0.25f},
        {0.25f, 0.75f},
        {0.75f, 0.75f},
    };

    SetWindowTextA(window_handle, "Ray Tracer - rendering...");

    for (int y = 0; y < IMAGE_HEIGHT; ++y) {
        for (int x = 0; x < IMAGE_WIDTH; ++x) {
            Vec3 color = vec3(0.0f, 0.0f, 0.0f);
            for (int s = 0; s < 4; ++s) {
                float u = ((float)x + offsets[s][0]) / (float)(IMAGE_WIDTH - 1);
                float v = 1.0f - (((float)y + offsets[s][1]) / (float)(IMAGE_HEIGHT - 1));
                color = add(color, trace_ray(camera_ray(u, v), MAX_DEPTH));
            }
            color = mul(color, 0.25f);
            pixels[y * IMAGE_WIDTH + x] = pack_bgra(color);
        }

        if ((y % 24) == 0) {
            InvalidateRect(window_handle, NULL, FALSE);
            UpdateWindow(window_handle);
        }
    }

    rendered = true;
    SetWindowTextA(window_handle, "Ray Tracer - R render  S save  Esc quit");
    InvalidateRect(window_handle, NULL, FALSE);
}

static void save_ppm(void)
{
    FILE *file = fopen("ray_tracer_output.ppm", "wb");
    if (file == NULL) {
        MessageBoxA(window_handle, "Could not save ray_tracer_output.ppm", "Ray Tracer", MB_ICONERROR);
        return;
    }

    fprintf(file, "P6\n%d %d\n255\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; ++i) {
        uint32_t p = pixels[i];
        unsigned char rgb[3] = {
            (unsigned char)((p >> 16) & 0xff),
            (unsigned char)((p >> 8) & 0xff),
            (unsigned char)(p & 0xff),
        };
        fwrite(rgb, 1, 3, file);
    }

    fclose(file);
    MessageBoxA(window_handle, "Saved ray_tracer_output.ppm", "Ray Tracer", MB_OK);
}

static void paint_image(HWND hwnd, HDC dc)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = IMAGE_WIDTH;
    info.bmiHeader.biHeight = -IMAGE_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        dc,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        0,
        0,
        IMAGE_WIDTH,
        IMAGE_HEIGHT,
        pixels,
        &info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        PostMessageA(hwnd, WM_APP + 1, 0, 0);
        return 0;

    case WM_APP + 1:
        render_image();
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wparam == 'R') {
            rendered = false;
            PostMessageA(hwnd, WM_APP + 1, 0, 0);
        } else if (wparam == 'S' && rendered) {
            save_ppm();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint_image(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;

    const char class_name[] = "CpuRayTracerWindow";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    RECT rect = {0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    window_handle = CreateWindowExA(
        0,
        class_name,
        "Ray Tracer",
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

    ShowWindow(window_handle, show_command);
    UpdateWindow(window_handle);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return (int)message.wParam;
}
