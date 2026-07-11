#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 600
#define GRID_W 160
#define GRID_H 100
#define SOLVER_STEPS 16
#define TIMER_ID 1
#define TIMER_MS 16
#define IX(x, y) ((x) + (GRID_W + 2) * (y))

static HWND window_handle;
static float *density;
static float *density_prev;
static float *u;
static float *v;
static float *u_prev;
static float *v_prev;
static uint32_t pixels[GRID_W * GRID_H];
static bool mouse_down = false;
static int mouse_x = 0;
static int mouse_y = 0;
static int prev_mouse_x = 0;
static int prev_mouse_y = 0;
static float dt = 0.10f;
static float diffusion = 0.00004f;
static float viscosity = 0.00002f;

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

static uint32_t pack_rgb(float r, float g, float b)
{
    r = clamp_float(r, 0.0f, 1.0f);
    g = clamp_float(g, 0.0f, 1.0f);
    b = clamp_float(b, 0.0f, 1.0f);
    return ((uint32_t)(r * 255.0f) << 16) | ((uint32_t)(g * 255.0f) << 8) | (uint32_t)(b * 255.0f);
}

static void clear_simulation(void)
{
    const size_t count = (GRID_W + 2) * (GRID_H + 2);
    memset(density, 0, sizeof(float) * count);
    memset(density_prev, 0, sizeof(float) * count);
    memset(u, 0, sizeof(float) * count);
    memset(v, 0, sizeof(float) * count);
    memset(u_prev, 0, sizeof(float) * count);
    memset(v_prev, 0, sizeof(float) * count);
}

static void set_boundary(int b, float *x)
{
    for (int i = 1; i <= GRID_W; ++i) {
        x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
        x[IX(i, GRID_H + 1)] = b == 2 ? -x[IX(i, GRID_H)] : x[IX(i, GRID_H)];
    }

    for (int j = 1; j <= GRID_H; ++j) {
        x[IX(0, j)] = b == 1 ? -x[IX(1, j)] : x[IX(1, j)];
        x[IX(GRID_W + 1, j)] = b == 1 ? -x[IX(GRID_W, j)] : x[IX(GRID_W, j)];
    }

    x[IX(0, 0)] = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
    x[IX(0, GRID_H + 1)] = 0.5f * (x[IX(1, GRID_H + 1)] + x[IX(0, GRID_H)]);
    x[IX(GRID_W + 1, 0)] = 0.5f * (x[IX(GRID_W, 0)] + x[IX(GRID_W + 1, 1)]);
    x[IX(GRID_W + 1, GRID_H + 1)] =
        0.5f * (x[IX(GRID_W, GRID_H + 1)] + x[IX(GRID_W + 1, GRID_H)]);
}

static void linear_solve(int b, float *x, const float *x0, float a, float c)
{
    for (int k = 0; k < SOLVER_STEPS; ++k) {
        for (int j = 1; j <= GRID_H; ++j) {
            for (int i = 1; i <= GRID_W; ++i) {
                x[IX(i, j)] =
                    (x0[IX(i, j)]
                     + a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] + x[IX(i, j - 1)] + x[IX(i, j + 1)]))
                    / c;
            }
        }
        set_boundary(b, x);
    }
}

static void diffuse(int b, float *x, const float *x0, float diff)
{
    float a = dt * diff * (float)GRID_W * (float)GRID_H;
    linear_solve(b, x, x0, a, 1.0f + 4.0f * a);
}

static void advect(int b, float *d, const float *d0, const float *u_field, const float *v_field)
{
    float dt_x = dt * (float)GRID_W;
    float dt_y = dt * (float)GRID_H;

    for (int j = 1; j <= GRID_H; ++j) {
        for (int i = 1; i <= GRID_W; ++i) {
            float x = (float)i - dt_x * u_field[IX(i, j)];
            float y = (float)j - dt_y * v_field[IX(i, j)];

            x = clamp_float(x, 0.5f, (float)GRID_W + 0.5f);
            y = clamp_float(y, 0.5f, (float)GRID_H + 0.5f);

            int i0 = (int)floorf(x);
            int i1 = i0 + 1;
            int j0 = (int)floorf(y);
            int j1 = j0 + 1;

            float s1 = x - (float)i0;
            float s0 = 1.0f - s1;
            float t1 = y - (float)j0;
            float t0 = 1.0f - t1;

            d[IX(i, j)] =
                s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)])
                + s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
        }
    }

    set_boundary(b, d);
}

static void project(float *u_field, float *v_field, float *p, float *div)
{
    for (int j = 1; j <= GRID_H; ++j) {
        for (int i = 1; i <= GRID_W; ++i) {
            div[IX(i, j)] =
                -0.5f
                * ((u_field[IX(i + 1, j)] - u_field[IX(i - 1, j)]) / (float)GRID_W
                   + (v_field[IX(i, j + 1)] - v_field[IX(i, j - 1)]) / (float)GRID_H);
            p[IX(i, j)] = 0.0f;
        }
    }

    set_boundary(0, div);
    set_boundary(0, p);
    linear_solve(0, p, div, 1.0f, 4.0f);

    for (int j = 1; j <= GRID_H; ++j) {
        for (int i = 1; i <= GRID_W; ++i) {
            u_field[IX(i, j)] -= 0.5f * (float)GRID_W * (p[IX(i + 1, j)] - p[IX(i - 1, j)]);
            v_field[IX(i, j)] -= 0.5f * (float)GRID_H * (p[IX(i, j + 1)] - p[IX(i, j - 1)]);
        }
    }

    set_boundary(1, u_field);
    set_boundary(2, v_field);
}

static void fade_density(void)
{
    for (int j = 1; j <= GRID_H; ++j) {
        for (int i = 1; i <= GRID_W; ++i) {
            density[IX(i, j)] *= 0.992f;
            u[IX(i, j)] *= 0.999f;
            v[IX(i, j)] *= 0.999f;
        }
    }
}

static void inject_at_mouse(void)
{
    if (!mouse_down) {
        return;
    }

    int gx = 1 + mouse_x * GRID_W / WINDOW_WIDTH;
    int gy = 1 + mouse_y * GRID_H / WINDOW_HEIGHT;
    gx = (int)clamp_float((float)gx, 2.0f, (float)GRID_W - 1.0f);
    gy = (int)clamp_float((float)gy, 2.0f, (float)GRID_H - 1.0f);

    float dx = (float)(mouse_x - prev_mouse_x);
    float dy = (float)(mouse_y - prev_mouse_y);

    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            int px = gx + x;
            int py = gy + y;
            float falloff = 1.0f - sqrtf((float)(x * x + y * y)) / 4.5f;
            if (falloff > 0.0f) {
                density[IX(px, py)] += 70.0f * falloff;
                u[IX(px, py)] += dx * 0.012f * falloff;
                v[IX(px, py)] += dy * 0.012f * falloff;
            }
        }
    }
}

static void step_simulation(void)
{
    inject_at_mouse();

    memcpy(u_prev, u, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));
    memcpy(v_prev, v, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));

    diffuse(1, u, u_prev, viscosity);
    diffuse(2, v, v_prev, viscosity);
    project(u, v, u_prev, v_prev);

    memcpy(u_prev, u, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));
    memcpy(v_prev, v, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));

    advect(1, u, u_prev, u_prev, v_prev);
    advect(2, v, v_prev, u_prev, v_prev);
    project(u, v, u_prev, v_prev);

    memcpy(density_prev, density, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));
    diffuse(0, density, density_prev, diffusion);
    memcpy(density_prev, density, sizeof(float) * (GRID_W + 2) * (GRID_H + 2));
    advect(0, density, density_prev, u, v);

    fade_density();
}

static void update_pixels(void)
{
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            float d = density[IX(x + 1, y + 1)] * 0.018f;
            float speed = sqrtf(u[IX(x + 1, y + 1)] * u[IX(x + 1, y + 1)] + v[IX(x + 1, y + 1)] * v[IX(x + 1, y + 1)]);

            float r = d * 0.72f + speed * 1.3f;
            float g = d * 0.36f + speed * 0.8f;
            float b = d * 1.10f + 0.05f;

            float vignette_x = ((float)x / (float)GRID_W - 0.5f);
            float vignette_y = ((float)y / (float)GRID_H - 0.5f);
            float vignette = 1.0f - 0.45f * (vignette_x * vignette_x + vignette_y * vignette_y);

            pixels[y * GRID_W + x] = pack_rgb(r * vignette, g * vignette, b * vignette);
        }
    }
}

static void paint(HWND hwnd, HDC dc)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = GRID_W;
    info.bmiHeader.biHeight = -GRID_H;
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
        GRID_W,
        GRID_H,
        pixels,
        &info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

static bool allocate_simulation(void)
{
    size_t count = (GRID_W + 2) * (GRID_H + 2);
    density = calloc(count, sizeof(float));
    density_prev = calloc(count, sizeof(float));
    u = calloc(count, sizeof(float));
    v = calloc(count, sizeof(float));
    u_prev = calloc(count, sizeof(float));
    v_prev = calloc(count, sizeof(float));
    return density && density_prev && u && v && u_prev && v_prev;
}

static void free_simulation(void)
{
    free(density);
    free(density_prev);
    free(u);
    free(v);
    free(u_prev);
    free(v_prev);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
        SetWindowTextA(hwnd, "Fluid Simulation - drag mouse to stir, R reset, Esc quit");
        return 0;

    case WM_TIMER:
        step_simulation();
        update_pixels();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        mouse_down = true;
        mouse_x = prev_mouse_x = GET_X_LPARAM(lparam);
        mouse_y = prev_mouse_y = GET_Y_LPARAM(lparam);
        SetCapture(hwnd);
        return 0;

    case WM_LBUTTONUP:
        mouse_down = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEMOVE:
        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;
        mouse_x = GET_X_LPARAM(lparam);
        mouse_y = GET_Y_LPARAM(lparam);
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wparam == 'R') {
            clear_simulation();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;

    if (!allocate_simulation()) {
        MessageBoxA(NULL, "Failed to allocate simulation memory.", "Fluid Simulation", MB_ICONERROR);
        return 1;
    }

    const char class_name[] = "FluidSimulationWindow";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        free_simulation();
        return 1;
    }

    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    window_handle = CreateWindowExA(
        0,
        class_name,
        "Fluid Simulation",
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
        free_simulation();
        return 1;
    }

    ShowWindow(window_handle, show_command);
    UpdateWindow(window_handle);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    free_simulation();
    return (int)message.wParam;
}
