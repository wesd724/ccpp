#include "life.h"
#include "patterns.h"
#include "terminal.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum {
    MIN_BOARD_WIDTH = 20,
    MAX_BOARD_WIDTH = 100,
    MIN_BOARD_HEIGHT = 8,
    MAX_BOARD_HEIGHT = 50,
    HUD_ROWS = 7
};

typedef struct TextBuffer {
    char *data;
    size_t length;
    size_t capacity;
} TextBuffer;

typedef struct App {
    LifeGrid grid;
    size_t cursor_x;
    size_t cursor_y;
    bool paused;
    bool quit;
    double speed;
    double accumulator;
    uint64_t random_seed;
    int escape_state;
    double escape_started;
} App;

static volatile sig_atomic_t interrupted = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static bool text_reserve(TextBuffer *buffer, size_t extra)
{
    if (extra <= buffer->capacity - buffer->length) {
        return true;
    }

    size_t capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
    while (capacity - buffer->length < extra) {
        if (capacity > SIZE_MAX / 2) {
            return false;
        }
        capacity *= 2;
    }

    char *data = realloc(buffer->data, capacity);
    if (data == NULL) {
        return false;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return true;
}

static bool text_append(TextBuffer *buffer, const char *text)
{
    const size_t length = strlen(text);
    if (!text_reserve(buffer, length)) {
        return false;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    return true;
}

static bool text_append_format(TextBuffer *buffer, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    const int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0 || !text_reserve(buffer, (size_t)needed + 1)) {
        va_end(arguments);
        return false;
    }

    vsnprintf(buffer->data + buffer->length,
              buffer->capacity - buffer->length, format, arguments);
    va_end(arguments);
    buffer->length += (size_t)needed;
    return true;
}

static size_t clamp_size(size_t value, size_t minimum, size_t maximum)
{
    if (value < minimum) {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

static void board_size_for_terminal(size_t columns, size_t rows,
                                    size_t *width, size_t *height)
{
    const size_t available_width = columns > 2 ? (columns - 2) / 2 : 0;
    const size_t available_height = rows > HUD_ROWS ? rows - HUD_ROWS : 0;
    *width = clamp_size(available_width, MIN_BOARD_WIDTH, MAX_BOARD_WIDTH);
    *height = clamp_size(available_height, MIN_BOARD_HEIGHT, MAX_BOARD_HEIGHT);
}

static void adjust_cursor_for_resize(App *app, size_t old_width, size_t old_height)
{
    if (app->grid.width >= old_width) {
        app->cursor_x += (app->grid.width - old_width) / 2;
    } else {
        const size_t removed = (old_width - app->grid.width) / 2;
        app->cursor_x = app->cursor_x > removed ? app->cursor_x - removed : 0;
    }

    if (app->grid.height >= old_height) {
        app->cursor_y += (app->grid.height - old_height) / 2;
    } else {
        const size_t removed = (old_height - app->grid.height) / 2;
        app->cursor_y = app->cursor_y > removed ? app->cursor_y - removed : 0;
    }

    if (app->cursor_x >= app->grid.width) {
        app->cursor_x = app->grid.width - 1;
    }
    if (app->cursor_y >= app->grid.height) {
        app->cursor_y = app->grid.height - 1;
    }
}

static bool resize_for_terminal(App *app)
{
    size_t columns = 0;
    size_t rows = 0;
    size_t width = 0;
    size_t height = 0;
    terminal_get_size(&columns, &rows);
    board_size_for_terminal(columns, rows, &width, &height);
    if (width == app->grid.width && height == app->grid.height) {
        return true;
    }

    const size_t old_width = app->grid.width;
    const size_t old_height = app->grid.height;
    if (!life_grid_resize(&app->grid, width, height)) {
        return false;
    }
    adjust_cursor_for_resize(app, old_width, old_height);
    return true;
}

static bool append_border(TextBuffer *frame, size_t width)
{
    if (!text_append(frame, "\x1b[90m+")) {
        return false;
    }
    for (size_t x = 0; x < width; ++x) {
        if (!text_append(frame, "--")) {
            return false;
        }
    }
    return text_append(frame, "+\x1b[0m\x1b[K\n");
}

static bool append_cell(TextBuffer *frame, const App *app, size_t x, size_t y)
{
    const uint8_t age = life_grid_age(&app->grid, x, y);
    const bool cursor = app->paused && x == app->cursor_x && y == app->cursor_y;
    if (cursor) {
        return text_append(frame, age == 0 ? "\x1b[30;47m..\x1b[0m"
                                           : "\x1b[30;103m[]\x1b[0m");
    }
    if (age == 0) {
        return text_append(frame, "  ");
    }
    if (age == 1) {
        return text_append(frame, "\x1b[96m[]\x1b[0m");
    }
    if (age <= 5) {
        return text_append(frame, "\x1b[92m[]\x1b[0m");
    }
    return text_append(frame, "\x1b[32m[]\x1b[0m");
}

static bool render_app(const App *app, TextBuffer *frame)
{
    frame->length = 0;
    const char *state = app->paused ? "PAUSED / EDIT" : "RUNNING";
    const char *boundary = app->grid.boundary == LIFE_BOUNDARY_WRAP ? "WRAP" : "DEAD";

    if (!text_append(frame, "\x1b[H\x1b[1;97mConway's Game of Life\x1b[0m  " ) ||
        !text_append_format(frame, "B3/S23  \x1b[1;93m[%s]\x1b[0m\x1b[K\n", state) ||
        !text_append_format(frame,
            "Gen %-7" PRIu64 " Pop %-5" PRIu64 " Born %-4" PRIu64
            " Died %-4" PRIu64 " Speed %.0f gen/s  Edge %s\x1b[K\n",
            app->grid.generation, app->grid.population, app->grid.births,
            app->grid.deaths, app->speed, boundary) ||
        !append_border(frame, app->grid.width)) {
        return false;
    }

    for (size_t y = 0; y < app->grid.height; ++y) {
        if (!text_append(frame, "\x1b[90m|\x1b[0m")) {
            return false;
        }
        for (size_t x = 0; x < app->grid.width; ++x) {
            if (!append_cell(frame, app, x, y)) {
                return false;
            }
        }
        if (!text_append(frame, "\x1b[90m|\x1b[0m\x1b[K\n")) {
            return false;
        }
    }

    return append_border(frame, app->grid.width) &&
           text_append(frame,
               "Space Pause  N Step  Arrows/HJKL Move  Enter Toggle  +/- Speed\x1b[K\n") &&
           text_append(frame,
               "R Random  C Clear  B Boundary  1-5 Place pattern  Q/Esc Quit\x1b[K\n") &&
           text_append(frame,
               "1 Block  2 Blinker  3 Glider  4 Pulsar  5 Gosper gun\x1b[K\x1b[J");
}

static void move_cursor(App *app, int delta_x, int delta_y)
{
    app->paused = true;
    const int width = (int)app->grid.width;
    const int height = (int)app->grid.height;
    int x = (int)app->cursor_x + delta_x;
    int y = (int)app->cursor_y + delta_y;
    x = (x % width + width) % width;
    y = (y % height + height) % height;
    app->cursor_x = (size_t)x;
    app->cursor_y = (size_t)y;
}

static void place_pattern(App *app, size_t pattern_index)
{
    const LifePattern *pattern = life_pattern_get(pattern_index);
    if (pattern == NULL) {
        return;
    }
    app->paused = true;
    life_pattern_place(&app->grid, pattern, app->cursor_x, app->cursor_y);
}

static void handle_character(App *app, unsigned char key)
{
    switch (key) {
    case 'q':
    case 'Q':
        app->quit = true;
        break;
    case ' ':
    case 'p':
    case 'P':
        app->paused = !app->paused;
        app->accumulator = 0.0;
        break;
    case 'n':
    case 'N':
    case '.':
        app->paused = true;
        life_grid_step(&app->grid);
        break;
    case 'r':
    case 'R':
        app->paused = true;
        app->random_seed += UINT64_C(0x9e3779b97f4a7c15);
        life_grid_randomize(&app->grid, app->random_seed, 0.24);
        break;
    case 'c':
    case 'C':
        app->paused = true;
        life_grid_clear(&app->grid);
        break;
    case 'b':
    case 'B':
        app->grid.boundary = app->grid.boundary == LIFE_BOUNDARY_DEAD
                                 ? LIFE_BOUNDARY_WRAP
                                 : LIFE_BOUNDARY_DEAD;
        break;
    case '\r':
    case '\n':
        app->paused = true;
        life_grid_toggle(&app->grid, app->cursor_x, app->cursor_y);
        break;
    case '+':
    case '=':
        app->speed = app->speed < 60.0 ? app->speed + 1.0 : 60.0;
        break;
    case '-':
    case '_':
        app->speed = app->speed > 1.0 ? app->speed - 1.0 : 1.0;
        break;
    case 'h':
    case 'H':
        move_cursor(app, -1, 0);
        break;
    case 'j':
    case 'J':
        move_cursor(app, 0, 1);
        break;
    case 'k':
    case 'K':
        move_cursor(app, 0, -1);
        break;
    case 'l':
    case 'L':
        move_cursor(app, 1, 0);
        break;
    default:
        if (key >= '1' && key <= '5') {
            place_pattern(app, (size_t)(key - '1'));
        }
        break;
    }
}

static void handle_key(App *app, unsigned char key, double now)
{
    if (app->escape_state == 1) {
        if (key == '[' || key == 'O') {
            app->escape_state = 2;
            return;
        }
        app->quit = true;
        app->escape_state = 0;
        return;
    }
    if (app->escape_state == 2) {
        // CSI parameters (for example Shift+Arrow and F5) are not commands.
        if (key >= 0x20 && key <= 0x3f) return;
        switch (key) {
        case 'A':
            move_cursor(app, 0, -1);
            break;
        case 'B':
            move_cursor(app, 0, 1);
            break;
        case 'C':
            move_cursor(app, 1, 0);
            break;
        case 'D':
            move_cursor(app, -1, 0);
            break;
        default:
            break;
        }
        app->escape_state = 0;
        return;
    }
    if (key == 0x1bU) {
        app->escape_state = 1;
        app->escape_started = now;
        return;
    }
    handle_character(app, key);
}

static void handle_input(App *app)
{
    unsigned char input[64];
    const size_t count = terminal_read(input, sizeof(input));
    const double now = terminal_now();
    for (size_t i = 0; i < count; ++i) {
        handle_key(app, input[i], now);
    }

    if (app->escape_state != 0 && now - app->escape_started >= 0.05) {
        if (app->escape_state == 1) app->quit = true;
        app->escape_state = 0;
    }
}

static uint64_t make_seed(void)
{
    struct timespec now = {0};
    clock_gettime(CLOCK_REALTIME, &now);
    return (uint64_t)now.tv_sec ^ ((uint64_t)now.tv_nsec << 21) ^
           ((uint64_t)getpid() << 32);
}

static void print_help(const char *program)
{
    printf("Usage: %s [--help]\n", program);
    puts("Interactive Conway's Game of Life using the B3/S23 rule.");
    puts("Run it in an ANSI-compatible interactive terminal.");
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    if (!terminal_begin()) {
        fputs("An interactive ANSI terminal is required.\n", stderr);
        return EXIT_FAILURE;
    }

    size_t columns = 0;
    size_t rows = 0;
    size_t width = 0;
    size_t height = 0;
    terminal_get_size(&columns, &rows);
    board_size_for_terminal(columns, rows, &width, &height);

    App app = {
        .cursor_x = width / 2,
        .cursor_y = height / 2,
        .speed = 8.0,
        .random_seed = make_seed()
    };
    if (!life_grid_init(&app.grid, width, height)) {
        terminal_end();
        fputs("Could not allocate the Life grid.\n", stderr);
        return EXIT_FAILURE;
    }

    life_pattern_place(&app.grid, life_pattern_get(3), app.cursor_x, app.cursor_y);

    TextBuffer frame = {0};
    double previous_time = terminal_now();
    double previous_render = previous_time - 1.0;
    double previous_resize_check = previous_time;
    bool ok = true;

    while (!app.quit && !interrupted && ok) {
        const double now = terminal_now();
        double delta = now - previous_time;
        previous_time = now;
        if (delta > 0.25) {
            delta = 0.25;
        }

        handle_input(&app);
        if (now - previous_resize_check >= 0.25) {
            ok = resize_for_terminal(&app);
            previous_resize_check = now;
        }

        if (!app.paused) {
            app.accumulator += delta;
            const double interval = 1.0 / app.speed;
            int steps = 0;
            while (app.accumulator >= interval && steps < 8) {
                life_grid_step(&app.grid);
                app.accumulator -= interval;
                ++steps;
            }
            if (steps == 8) {
                app.accumulator = 0.0;
            }
        }

        if (now - previous_render >= 1.0 / 30.0) {
            ok = render_app(&app, &frame) &&
                 terminal_write(frame.data, frame.length);
            previous_render = now;
        }
        terminal_sleep_ms(4);
    }

    free(frame.data);
    life_grid_destroy(&app.grid);
    terminal_end();
    if (!ok) {
        fputs("The simulation stopped because a terminal operation failed.\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
