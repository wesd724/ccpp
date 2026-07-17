#include "patterns.h"

#include <stdbool.h>

static const LifePattern PATTERNS[] = {
    {
        .name = "Block",
        .width = 2,
        .height = 2,
        .cells =
            "OO"
            "OO"
    },
    {
        .name = "Blinker",
        .width = 3,
        .height = 1,
        .cells = "OOO"
    },
    {
        .name = "Glider",
        .width = 3,
        .height = 3,
        .cells =
            ".O."
            "..O"
            "OOO"
    },
    {
        .name = "Pulsar",
        .width = 13,
        .height = 13,
        .cells =
            "..OOO...OOO.."
            "............."
            "O....O.O....O"
            "O....O.O....O"
            "O....O.O....O"
            "..OOO...OOO.."
            "............."
            "..OOO...OOO.."
            "O....O.O....O"
            "O....O.O....O"
            "O....O.O....O"
            "............."
            "..OOO...OOO.."
    },
    {
        .name = "Gosper glider gun",
        .width = 36,
        .height = 9,
        .cells =
            "........................O..........."
            "......................O.O..........."
            "............OO......OO............OO"
            "...........O...O....OO............OO"
            "OO........O.....O...OO.............."
            "OO........O...O.OO....O.O..........."
            "..........O.....O.......O..........."
            "...........O...O...................."
            "............OO......................"
    }
};

size_t life_pattern_count(void)
{
    return sizeof(PATTERNS) / sizeof(PATTERNS[0]);
}

const LifePattern *life_pattern_get(size_t index)
{
    return index < life_pattern_count() ? &PATTERNS[index] : NULL;
}

size_t life_pattern_live_count(const LifePattern *pattern)
{
    if (pattern == NULL || pattern->cells == NULL) {
        return 0;
    }

    size_t count = 0;
    const size_t area = pattern->width * pattern->height;
    for (size_t i = 0; i < area; ++i) {
        count += pattern->cells[i] == 'O' ? 1U : 0U;
    }
    return count;
}

void life_pattern_place(LifeGrid *grid, const LifePattern *pattern,
                        size_t center_x, size_t center_y)
{
    if (grid == NULL || pattern == NULL || pattern->cells == NULL) {
        return;
    }

    const ptrdiff_t start_x = (ptrdiff_t)center_x - (ptrdiff_t)(pattern->width / 2);
    const ptrdiff_t start_y = (ptrdiff_t)center_y - (ptrdiff_t)(pattern->height / 2);

    for (size_t y = 0; y < pattern->height; ++y) {
        for (size_t x = 0; x < pattern->width; ++x) {
            if (pattern->cells[y * pattern->width + x] != 'O') {
                continue;
            }

            const ptrdiff_t target_x = start_x + (ptrdiff_t)x;
            const ptrdiff_t target_y = start_y + (ptrdiff_t)y;
            if (target_x >= 0 && target_y >= 0 &&
                target_x < (ptrdiff_t)grid->width &&
                target_y < (ptrdiff_t)grid->height) {
                life_grid_set(grid, (size_t)target_x, (size_t)target_y, true);
            }
        }
    }
}
