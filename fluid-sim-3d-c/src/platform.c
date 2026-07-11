#include "platform.h"

#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include <windowsx.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef HGLRC(WINAPI *WglCreateContextAttribsProc)(HDC, HGLRC, const int *);
typedef BOOL(WINAPI *WglSwapIntervalProc)(int);

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    Platform *platform = (Platform *)GetWindowLongPtrA(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTA *create = (CREATESTRUCTA *)lparam;
        platform = create->lpCreateParams;
        SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)platform);
    }
    if (!platform) {
        return DefWindowProcA(window, message, wparam, lparam);
    }

    InputState *input = &platform->input;
    switch (message) {
    case WM_CLOSE:
        input->quit = true;
        return 0;
    case WM_DESTROY:
        input->quit = true;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        input->width = LOWORD(lparam);
        input->height = HIWORD(lparam);
        return 0;
    case WM_LBUTTONDOWN:
        input->mouse_down = true;
        input->mouse_pressed = true;
        input->mouse_x = GET_X_LPARAM(lparam);
        input->mouse_y = GET_Y_LPARAM(lparam);
        SetCapture(window);
        return 0;
    case WM_LBUTTONUP:
        input->mouse_down = false;
        input->mouse_released = true;
        if (!input->look_down) ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        input->look_down = true;
        input->mouse_x = GET_X_LPARAM(lparam);
        input->mouse_y = GET_Y_LPARAM(lparam);
        SetCapture(window);
        return 0;
    case WM_RBUTTONUP:
        input->look_down = false;
        if (!input->mouse_down) ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        if (input->look_down) {
            input->mouse_delta_x += x - input->mouse_x;
            input->mouse_delta_y += y - input->mouse_y;
        }
        input->mouse_x = x;
        input->mouse_y = y;
        return 0;
    }
    case WM_MOUSEWHEEL:
        input->wheel_delta += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
        return 0;
    case WM_CAPTURECHANGED:
        input->mouse_down = false;
        input->look_down = false;
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) input->quit = true;
        if (wparam < 256) input->keys[wparam] = true;
        return 0;
    case WM_KEYUP:
        if (wparam < 256) input->keys[wparam] = false;
        return 0;
    default:
        return DefWindowProcA(window, message, wparam, lparam);
    }
}

static HGLRC create_modern_context(HDC device) {
    HGLRC temporary = wglCreateContext(device);
    if (!temporary || !wglMakeCurrent(device, temporary)) {
        return NULL;
    }
    PROC raw_create_context = wglGetProcAddress("wglCreateContextAttribsARB");
    WglCreateContextAttribsProc create_context = NULL;
    memcpy(&create_context, &raw_create_context, sizeof(create_context));
    if (!create_context) {
        return temporary;
    }

    const int versions[][2] = {{4, 6}, {4, 5}, {4, 3}, {3, 3}};
    HGLRC modern = NULL;
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]) && !modern; ++i) {
        const int attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, versions[i][0],
            WGL_CONTEXT_MINOR_VERSION_ARB, versions[i][1],
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0,
        };
        modern = create_context(device, NULL, attributes);
    }
    if (!modern) {
        return temporary;
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(temporary);
    if (!wglMakeCurrent(device, modern)) {
        wglDeleteContext(modern);
        return NULL;
    }
    return modern;
}

bool platform_init(Platform *platform, HINSTANCE instance, int width, int height, const char *title) {
    memset(platform, 0, sizeof(*platform));
    platform->input.width = width;
    platform->input.height = height;
    SetProcessDPIAware();

    WNDCLASSA window_class = {0};
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = "FluidSim3DCoreWindow";
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT bounds = {0, 0, width, height};
    AdjustWindowRect(&bounds, style, FALSE);
    platform->window = CreateWindowExA(
        0, window_class.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
        NULL, NULL, instance, platform);
    if (!platform->window) {
        return false;
    }

    platform->device = GetDC(platform->window);
    PIXELFORMATDESCRIPTOR format = {
        .nSize = sizeof(format),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };
    int pixel_format = ChoosePixelFormat(platform->device, &format);
    if (!pixel_format || !SetPixelFormat(platform->device, pixel_format, &format)) {
        return false;
    }
    platform->context = create_modern_context(platform->device);
    if (!platform->context) return false;

    PROC raw_swap_interval = wglGetProcAddress("wglSwapIntervalEXT");
    WglSwapIntervalProc swap_interval = NULL;
    memcpy(&swap_interval, &raw_swap_interval, sizeof(swap_interval));
    platform->vsync_enabled = swap_interval && swap_interval(1);
    return true;
}

void platform_poll(Platform *platform) {
    platform->input.mouse_pressed = false;
    platform->input.mouse_released = false;
    platform->input.mouse_delta_x = 0;
    platform->input.mouse_delta_y = 0;
    platform->input.wheel_delta = 0.0f;
    MSG message;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    if (!IsWindow(platform->window)) {
        platform->input.quit = true;
    }
}

void platform_swap(const Platform *platform) {
    SwapBuffers(platform->device);
}

void platform_shutdown(Platform *platform) {
    wglMakeCurrent(NULL, NULL);
    if (platform->context) wglDeleteContext(platform->context);
    if (platform->device && platform->window) ReleaseDC(platform->window, platform->device);
    if (platform->window) DestroyWindow(platform->window);
    memset(platform, 0, sizeof(*platform));
}
