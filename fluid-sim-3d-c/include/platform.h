#ifndef PLATFORM_H
#define PLATFORM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>

typedef struct {
    bool keys[256];
    bool mouse_down;
    bool mouse_pressed;
    bool mouse_released;
    bool look_down;
    bool quit;
    int mouse_x;
    int mouse_y;
    int mouse_delta_x;
    int mouse_delta_y;
    float wheel_delta;
    int width;
    int height;
} InputState;

typedef struct {
    HWND window;
    HDC device;
    HGLRC context;
    bool vsync_enabled;
    InputState input;
} Platform;

bool platform_init(Platform *platform, HINSTANCE instance, int width, int height, const char *title);
void platform_poll(Platform *platform);
void platform_swap(const Platform *platform);
void platform_shutdown(Platform *platform);

#endif
