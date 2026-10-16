#include "test.h"
#include "../fluid-sim-c/main.c"

int main(void)
{
    enum { GUARD = 512, COUNT = (GRID_W + 2) * (GRID_H + 2) };
    float *storage[3];
    for (int i = 0; i < 3; ++i) {
        storage[i] = calloc(COUNT + 2 * GUARD, sizeof(float));
        CHECK(storage[i] != NULL);
        for (int j = 0; j < GUARD; ++j) {
            storage[i][j] = storage[i][COUNT + GUARD + j] = -1234.0f;
        }
    }
    density = storage[0] + GUARD;
    u = storage[1] + GUARD;
    v = storage[2] + GUARD;
    mouse_down = true;
    const int corners[][2] = {{0, 0}, {959, 0}, {0, 599}, {959, 599}, {-400, -400}, {2000, 2000}};
    for (size_t corner = 0; corner < sizeof(corners) / sizeof(corners[0]); ++corner) {
        mouse_x = corners[corner][0];
        mouse_y = corners[corner][1];
        prev_mouse_x = mouse_x - 10;
        prev_mouse_y = mouse_y - 5;
        inject_at_mouse();
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < GUARD; ++j) {
                CHECK(storage[i][j] == -1234.0f);
                CHECK(storage[i][COUNT + GUARD + j] == -1234.0f);
            }
            const float *field = storage[i] + GUARD;
            for (int y = 0; y <= GRID_H + 1; ++y) {
                CHECK(field[IX(0, y)] == 0 && field[IX(GRID_W + 1, y)] == 0);
            }
            for (int x = 0; x <= GRID_W + 1; ++x) {
                CHECK(field[IX(x, 0)] == 0 && field[IX(x, GRID_H + 1)] == 0);
            }
        }
    }
    const float velocity = u[IX(GRID_W, GRID_H)];
    inject_at_mouse();
    CHECK(u[IX(GRID_W, GRID_H)] == velocity);
    window_proc(NULL, WM_CAPTURECHANGED, 0, 0);
    CHECK(!mouse_down);
    mouse_down = true;
    window_proc(NULL, WM_SIZE, 0, MAKELPARAM(480, 300));
    mouse_x = prev_mouse_x = 240;
    mouse_y = prev_mouse_y = 150;
    const float before = density[IX(81, 51)];
    inject_at_mouse();
    CHECK(density[IX(81, 51)] > before);
    window_proc(NULL, WM_SIZE, 0, 0);
    inject_at_mouse();
    for (int i = 0; i < 3; ++i) free(storage[i]);
    puts("Fluid brush guards, consumed motion and capture loss passed.");
    return 0;
}
