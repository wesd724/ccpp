#include "test.h"
#include "../opengl-shooter-3d-c/main.c"

int main(void)
{
    keys['W'] = true;
    mouse_down = true;
    window_proc(NULL, WM_KILLFOCUS, 0, 0);
    CHECK(!keys['W'] && !mouse_down);
    CHECK(!key_is_down(-1) && !key_is_down(256));
    puts("3D shooter focus and key range passed.");
    return 0;
}
