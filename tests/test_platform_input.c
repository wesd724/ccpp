#include "test.h"
#include "../fluid-sim-3d-c/src/platform.c"

int main(void)
{
    Platform platform = {0};
    HWND window = CreateWindowExA(0, "STATIC", "Input regression", WS_POPUP,
                                  0, 0, 100, 100, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(window != NULL);
    SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)&platform);
    platform.input.keys['W'] = true;
    platform.input.mouse_down = true;
    platform.input.look_down = true;
    platform.input.mouse_delta_x = 42;
    window_proc(window, WM_KILLFOCUS, 0, 0);
    CHECK(!platform.input.keys['W']);
    CHECK(!platform.input.mouse_down && !platform.input.look_down);
    CHECK(platform.input.mouse_released);
    CHECK(platform.input.mouse_delta_x == 0);
    platform.input.mouse_released = false;
    platform.input.mouse_down = true;
    window_proc(window, WM_CAPTURECHANGED, 0, 0);
    CHECK(platform.input.mouse_released);
    DestroyWindow(window);
    puts("3D fluid focus and drag cancellation passed.");
    return 0;
}
