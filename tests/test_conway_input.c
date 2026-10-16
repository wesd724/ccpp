#include "test.h"
#define main conway_application_main
#include "../conways-game-of-life-c/src/main.c"
#undef main

static void send_keys(App *app, const char *keys)
{
    for (size_t i = 0; keys[i]; ++i) handle_key(app, (unsigned char)keys[i], 1.0);
}

int main(void)
{
    App app = {0};
    CHECK(life_grid_init(&app.grid, 40, 20));
    app.cursor_x = 20;
    app.cursor_y = 10;
    send_keys(&app, "\x1b[15~");
    CHECK(!app.quit && app.grid.population == 0);
    send_keys(&app, "\x1b[1;2D");
    CHECK(!app.quit && app.cursor_x == 19);
    send_keys(&app, "\x1bOC");
    CHECK(!app.quit && app.cursor_x == 20);
    send_keys(&app, "\x1b[A");
    CHECK(app.cursor_y == 9);
    life_grid_destroy(&app.grid);
    puts("Conway CSI, modified arrows and application cursor sequences passed.");
    return 0;
}
