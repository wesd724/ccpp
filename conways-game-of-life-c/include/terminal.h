#ifndef CONWAYS_TERMINAL_H
#define CONWAYS_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

bool terminal_begin(void);
void terminal_end(void);
void terminal_get_size(size_t *columns, size_t *rows);
size_t terminal_read(unsigned char *buffer, size_t capacity);
bool terminal_write(const char *data, size_t length);
double terminal_now(void);
void terminal_sleep_ms(long milliseconds);

#endif
