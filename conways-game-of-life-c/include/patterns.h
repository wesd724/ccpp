#ifndef CONWAYS_PATTERNS_H
#define CONWAYS_PATTERNS_H

#include "life.h"

#include <stddef.h>

typedef struct LifePattern {
    const char *name;
    size_t width;
    size_t height;
    const char *cells;
} LifePattern;

size_t life_pattern_count(void);
const LifePattern *life_pattern_get(size_t index);
size_t life_pattern_live_count(const LifePattern *pattern);
void life_pattern_place(LifeGrid *grid, const LifePattern *pattern,
                        size_t center_x, size_t center_y);

#endif
