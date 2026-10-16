#include "test.h"
#include "../ray-tracer-c/main.c"

int main(void)
{
    const Ray left = camera_ray(0.25f, 0.5f);
    const Ray right = camera_ray(0.75f, 0.5f);
    const Ray top = camera_ray(0.5f, 0.75f);
    const Ray bottom = camera_ray(0.5f, 0.25f);
    CHECK(left.direction.x < right.direction.x);
    CHECK(top.direction.y > bottom.direction.y);
    CHECK(fabsf(length(left.direction) - 1.0f) < 0.0001f);
    Hit hit = {0};
    const Ray above = {add(spheres[0].center, vec3(0, 2, 0)), {0, -1, 0}};
    CHECK(hit_sphere(above, &spheres[0], EPSILON, 1000, &hit));
    CHECK(fabsf(hit.t - 1.25f) < 0.0001f);
    CHECK(hit.material.color.x == spheres[0].material.color.x);
    CHECK(!hit.material.checkerboard);
    const Ray parallel = {{0, 1, 0}, {1, 0, 0}};
    CHECK(!hit_plane(parallel, &floor_plane, EPSILON, 1000, &hit));
    const Ray down = {{0, 2, 0}, {0, -1, 0}};
    CHECK(hit_plane(down, &floor_plane, EPSILON, 1000, &hit));
    CHECK(hit.material.checkerboard);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "RayBatchRegression";
    CHECK(RegisterClassA(&wc) != 0);
    window_handle = CreateWindowExA(0, wc.lpszClassName, "Ray regression",
        WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, NULL, NULL, wc.hInstance, NULL);
    CHECK(window_handle != NULL);
    MSG message;
    CHECK(PeekMessageA(&message, window_handle, WM_APP + 1, WM_APP + 1, PM_REMOVE));
    DispatchMessageA(&message);
    CHECK(render_row == 8 && !rendered);
    SendMessageA(window_handle, WM_KEYDOWN, VK_ESCAPE, 0);
    CHECK(!IsWindow(window_handle));
    puts("Ray camera handedness, normalized rays and intersections passed.");
    return 0;
}
