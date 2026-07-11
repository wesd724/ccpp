#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 700
#define TIMER_ID 1
#define TIMER_MS 16
#define TRAIL_COUNT 900
#define SUB_STEPS 4

typedef struct {
    double theta1;
    double theta2;
    double omega1;
    double omega2;
} State;

typedef struct {
    int x;
    int y;
} Point2i;

static const double gravity = 9.81;
static const double mass1 = 1.0;
static const double mass2 = 1.0;
static const double length1 = 1.0;
static const double length2 = 1.0;
static const double dt = 0.006;

static State state = {1.65, 2.35, 0.0, 0.0};
static Point2i trail[TRAIL_COUNT];
static int trail_head = 0;
static int trail_size = 0;
static bool paused = false;

static State add_scaled(State a, State b, double scale)
{
    return (State){
        a.theta1 + b.theta1 * scale,
        a.theta2 + b.theta2 * scale,
        a.omega1 + b.omega1 * scale,
        a.omega2 + b.omega2 * scale,
    };
}

static State derivatives(State s)
{
    const double delta = s.theta1 - s.theta2;
    const double denominator = 2.0 * mass1 + mass2 - mass2 * cos(2.0 * delta);

    const double alpha1 =
        (-gravity * (2.0 * mass1 + mass2) * sin(s.theta1)
         - mass2 * gravity * sin(s.theta1 - 2.0 * s.theta2)
         - 2.0 * sin(delta) * mass2
               * (s.omega2 * s.omega2 * length2
                  + s.omega1 * s.omega1 * length1 * cos(delta)))
        / (length1 * denominator);

    const double alpha2 =
        (2.0 * sin(delta)
         * (s.omega1 * s.omega1 * length1 * (mass1 + mass2)
            + gravity * (mass1 + mass2) * cos(s.theta1)
            + s.omega2 * s.omega2 * length2 * mass2 * cos(delta)))
        / (length2 * denominator);

    return (State){s.omega1, s.omega2, alpha1, alpha2};
}

static void integrate(void)
{
    const State k1 = derivatives(state);
    const State k2 = derivatives(add_scaled(state, k1, dt * 0.5));
    const State k3 = derivatives(add_scaled(state, k2, dt * 0.5));
    const State k4 = derivatives(add_scaled(state, k3, dt));

    state.theta1 += dt / 6.0 * (k1.theta1 + 2.0 * k2.theta1 + 2.0 * k3.theta1 + k4.theta1);
    state.theta2 += dt / 6.0 * (k1.theta2 + 2.0 * k2.theta2 + 2.0 * k3.theta2 + k4.theta2);
    state.omega1 += dt / 6.0 * (k1.omega1 + 2.0 * k2.omega1 + 2.0 * k3.omega1 + k4.omega1);
    state.omega2 += dt / 6.0 * (k1.omega2 + 2.0 * k2.omega2 + 2.0 * k3.omega2 + k4.omega2);
}

static void reset_simulation(void)
{
    state = (State){1.65, 2.35, 0.0, 0.0};
    trail_head = 0;
    trail_size = 0;
}

static void pendulum_points(RECT rect, Point2i *origin, Point2i *bob1, Point2i *bob2)
{
    const int cx = (rect.right - rect.left) / 2;
    const int cy = 115;
    const double pixel_length = 185.0;

    origin->x = cx;
    origin->y = cy;
    bob1->x = origin->x + (int)(sin(state.theta1) * pixel_length);
    bob1->y = origin->y + (int)(cos(state.theta1) * pixel_length);
    bob2->x = bob1->x + (int)(sin(state.theta2) * pixel_length);
    bob2->y = bob1->y + (int)(cos(state.theta2) * pixel_length);
}

static void push_trail(Point2i point)
{
    trail[trail_head] = point;
    trail_head = (trail_head + 1) % TRAIL_COUNT;
    if (trail_size < TRAIL_COUNT) {
        ++trail_size;
    }
}

static HPEN make_pen(COLORREF color, int width)
{
    return CreatePen(PS_SOLID, width, color);
}

static HBRUSH make_brush(COLORREF color)
{
    return CreateSolidBrush(color);
}

static void draw_filled_circle(HDC dc, Point2i center, int radius, HBRUSH brush)
{
    HBRUSH old_brush = SelectObject(dc, brush);
    HPEN old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, center.x - radius, center.y - radius, center.x + radius, center.y + radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
}

static void draw_scene(HWND hwnd, HDC window_dc)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    const int w = rect.right - rect.left;
    const int h = rect.bottom - rect.top;

    HDC dc = CreateCompatibleDC(window_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(window_dc, w, h);
    HBITMAP old_bitmap = SelectObject(dc, bitmap);

    HBRUSH background = make_brush(RGB(10, 12, 18));
    FillRect(dc, &rect, background);
    DeleteObject(background);

    Point2i origin;
    Point2i bob1;
    Point2i bob2;
    pendulum_points(rect, &origin, &bob1, &bob2);

    if (trail_size > 1) {
        for (int i = 1; i < trail_size; ++i) {
            const int prev_index = (trail_head - trail_size + i - 1 + TRAIL_COUNT) % TRAIL_COUNT;
            const int index = (trail_head - trail_size + i + TRAIL_COUNT) % TRAIL_COUNT;
            const int alpha = 40 + (180 * i) / trail_size;
            HPEN trail_pen = make_pen(RGB(30, alpha, 255), 2);
            HPEN old_pen = SelectObject(dc, trail_pen);
            MoveToEx(dc, trail[prev_index].x, trail[prev_index].y, NULL);
            LineTo(dc, trail[index].x, trail[index].y);
            SelectObject(dc, old_pen);
            DeleteObject(trail_pen);
        }
    }

    HPEN arm_pen = make_pen(RGB(218, 222, 230), 4);
    HPEN old_pen = SelectObject(dc, arm_pen);
    MoveToEx(dc, origin.x, origin.y, NULL);
    LineTo(dc, bob1.x, bob1.y);
    LineTo(dc, bob2.x, bob2.y);
    SelectObject(dc, old_pen);
    DeleteObject(arm_pen);

    HBRUSH pivot_brush = make_brush(RGB(245, 245, 245));
    HBRUSH bob1_brush = make_brush(RGB(255, 191, 71));
    HBRUSH bob2_brush = make_brush(RGB(76, 201, 240));
    draw_filled_circle(dc, origin, 7, pivot_brush);
    draw_filled_circle(dc, bob1, 18, bob1_brush);
    draw_filled_circle(dc, bob2, 22, bob2_brush);
    DeleteObject(pivot_brush);
    DeleteObject(bob1_brush);
    DeleteObject(bob2_brush);

    BitBlt(window_dc, 0, 0, w, h, dc, 0, 0, SRCCOPY);

    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
        return 0;

    case WM_TIMER:
        if (!paused) {
            for (int i = 0; i < SUB_STEPS; ++i) {
                integrate();
            }

            RECT rect;
            Point2i origin;
            Point2i bob1;
            Point2i bob2;
            GetClientRect(hwnd, &rect);
            pendulum_points(rect, &origin, &bob1, &bob2);
            push_trail(bob2);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wparam == ' ') {
            paused = !paused;
        } else if (wparam == 'R') {
            reset_simulation();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        draw_scene(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;

    const char class_name[] = "DoublePendulumWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowEx(
        0,
        class_name,
        "Double Pendulum",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL) {
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return (int)message.wParam;
}
