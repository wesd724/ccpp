#include "terminal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static struct termios original_settings;
static bool terminal_active = false;

bool terminal_begin(void)
{
    if (terminal_active) {
        return true;
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
        tcgetattr(STDIN_FILENO, &original_settings) != 0) {
        return false;
    }

    struct termios raw = original_settings;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return false;
    }

    terminal_active = true;
    atexit(terminal_end);
    fputs("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l", stdout);
    fflush(stdout);
    return true;
}

void terminal_end(void)
{
    if (!terminal_active) {
        return;
    }

    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_settings);
    terminal_active = false;
}

void terminal_get_size(size_t *columns, size_t *rows)
{
    struct winsize size = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 &&
        size.ws_col > 0 && size.ws_row > 0) {
        *columns = size.ws_col;
        *rows = size.ws_row;
        return;
    }

    *columns = 80;
    *rows = 24;
}

size_t terminal_read(unsigned char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) {
        return 0;
    }

    const ssize_t count = read(STDIN_FILENO, buffer, capacity);
    if (count <= 0) {
        return 0;
    }
    return (size_t)count;
}

bool terminal_write(const char *data, size_t length)
{
    return data != NULL && fwrite(data, 1, length, stdout) == length &&
           fflush(stdout) == 0;
}

double terminal_now(void)
{
    struct timespec now = {0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

void terminal_sleep_ms(long milliseconds)
{
    if (milliseconds <= 0) {
        return;
    }

    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}
