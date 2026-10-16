#include "test.h"
#include "../opengl-shooter-c/main.c"

int main(void)
{
    keys['A'] = true;
    mouse_down = true;
    window_proc(NULL, WM_KILLFOCUS, 0, 0);
    CHECK(!keys['A'] && !mouse_down);
    CHECK(!key_is_down(-1) && !key_is_down(256));
    mouse_down = true;
    window_proc(NULL, WM_CAPTURECHANGED, 0, 0);
    CHECK(!mouse_down);
    window_proc(NULL, WM_SIZE, 0, MAKELPARAM(500, 360));
    window_proc(NULL, WM_MOUSEMOVE, 0, MAKELPARAM(250, 180));
    CHECK(mouse_pos.x == WINDOW_WIDTH * 0.5f && mouse_pos.y == WINDOW_HEIGHT * 0.5f);
    puts("2D shooter focus, capture and key range passed.");
    return 0;
}
