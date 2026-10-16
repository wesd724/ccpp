#include "test.h"
#include "../opengl-shooter-c/main.c"

int main(void)
{
    WNDCLASSA wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "CcppGraphicsRegression";
    CHECK(RegisterClassA(&wc) != 0);
    window_handle = CreateWindowExA(0, wc.lpszClassName, "Graphics regression",
        WS_POPUP, 0, 0, 64, 64, NULL, NULL, wc.hInstance, NULL);
    CHECK(window_handle != NULL);
    CHECK(initialize_opengl(window_handle));
    printf("Graphics driver: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) CHECK(mgl_init());
        glViewport(0, 0, 64, 64);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        mglColor3f(1, 0, 0);
        mglBegin(GL_TRIANGLES);
        mglVertex3f(-1, -1, 0);
        mglVertex3f(1, -1, 0);
        mglVertex3f(0, 1, 0);
        mglEnd();
        unsigned char pixel[4] = {0};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        CHECK(pixel[0] > 240 && pixel[1] < 10 && pixel[2] < 10);
        CHECK(glGetError() == GL_NO_ERROR);
        mgl_shutdown();
        CHECK(!mgl_ready && !mgl_program && !mgl_vao && !mgl_vbo);
    }
    shutdown_opengl();
    DestroyWindow(window_handle);
    puts("OpenGL compile/link, triangle pixel output, cleanup and reinitialization passed.");
    return 0;
}
