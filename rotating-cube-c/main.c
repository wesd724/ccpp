#define _POSIX_C_SOURCE 199309L

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 80
#define HEIGHT 40
#define SURFACE_STEP 0.18f
#define CUBE_SIZE 1.0f
#define CAMERA_DISTANCE 4.0f
#define SCALE 28.0f
#define FRAME_DELAY_NS 30000000L

static float zbuffer[WIDTH * HEIGHT];
static char screen[WIDTH * HEIGHT];
static volatile sig_atomic_t running = 1;

static void restore_terminal(void)
{
    printf("\x1b[?25h\x1b[0m\n");
    fflush(stdout);
}

static void handle_signal(int signum)
{
    (void)signum;
    running = 0;
}

static void rotate_point(float x, float y, float z, float ax, float ay, float az,
                         float *rx, float *ry, float *rz)
{
    float sin_x = sinf(ax);
    float cos_x = cosf(ax);
    float sin_y = sinf(ay);
    float cos_y = cosf(ay);
    float sin_z = sinf(az);
    float cos_z = cosf(az);

    float y1 = y * cos_x - z * sin_x;
    float z1 = y * sin_x + z * cos_x;
    float x1 = x;

    float x2 = x1 * cos_y + z1 * sin_y;
    float z2 = -x1 * sin_y + z1 * cos_y;
    float y2 = y1;

    *rx = x2 * cos_z - y2 * sin_z;
    *ry = x2 * sin_z + y2 * cos_z;
    *rz = z2;
}

static void plot_point(float x, float y, float z, char shade)
{
    float depth = z + CAMERA_DISTANCE;
    if (depth <= 0.1f) {
        return;
    }

    int sx = (int)(WIDTH / 2.0f + (x / depth) * SCALE * 2.0f);
    int sy = (int)(HEIGHT / 2.0f - (y / depth) * SCALE);

    if (sx < 0 || sx >= WIDTH || sy < 0 || sy >= HEIGHT) {
        return;
    }

    int index = sy * WIDTH + sx;
    float inv_depth = 1.0f / depth;
    if (inv_depth > zbuffer[index]) {
        zbuffer[index] = inv_depth;
        screen[index] = shade;
    }
}

static void draw_cube_face(float ax, float ay, float az, int face)
{
    for (float u = -CUBE_SIZE; u <= CUBE_SIZE; u += SURFACE_STEP) {
        for (float v = -CUBE_SIZE; v <= CUBE_SIZE; v += SURFACE_STEP) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            char shade = '#';

            switch (face) {
            case 0:
                x = CUBE_SIZE;
                y = u;
                z = v;
                shade = '@';
                break;
            case 1:
                x = -CUBE_SIZE;
                y = u;
                z = v;
                shade = '%';
                break;
            case 2:
                x = u;
                y = CUBE_SIZE;
                z = v;
                shade = '#';
                break;
            case 3:
                x = u;
                y = -CUBE_SIZE;
                z = v;
                shade = '*';
                break;
            case 4:
                x = u;
                y = v;
                z = CUBE_SIZE;
                shade = '+';
                break;
            case 5:
                x = u;
                y = v;
                z = -CUBE_SIZE;
                shade = '.';
                break;
            }

            float rx, ry, rz;
            rotate_point(x, y, z, ax, ay, az, &rx, &ry, &rz);
            plot_point(rx, ry, rz, shade);
        }
    }
}

static void render_frame(float ax, float ay, float az)
{
    memset(screen, ' ', sizeof(screen));
    memset(zbuffer, 0, sizeof(zbuffer));

    for (int face = 0; face < 6; ++face) {
        draw_cube_face(ax, ay, az, face);
    }

    printf("\x1b[H");
    for (int y = 0; y < HEIGHT; ++y) {
        fwrite(&screen[y * WIDTH], 1, WIDTH, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

int main(void)
{
    const struct timespec frame_delay = {0, FRAME_DELAY_NS};

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(restore_terminal);

    printf("\x1b[2J\x1b[?25l");

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;

    while (running) {
        render_frame(ax, ay, az);
        ax += 0.045f;
        ay += 0.030f;
        az += 0.020f;
        nanosleep(&frame_delay, NULL);
    }

    return 0;
}
