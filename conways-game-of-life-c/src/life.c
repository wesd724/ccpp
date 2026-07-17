#include "life.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool valid_area(size_t width, size_t height, size_t *area)
{
    if (width == 0 || height == 0 || width > SIZE_MAX / height) {
        return false;
    }

    *area = width * height;
    return true;
}

static size_t cell_index(const LifeGrid *grid, size_t x, size_t y)
{
    return y * grid->width + x;
}

bool life_grid_init(LifeGrid *grid, size_t width, size_t height)
{
    size_t area = 0;
    if (grid == NULL || !valid_area(width, height, &area)) {
        return false;
    }

    uint8_t *cells = calloc(area, sizeof(*cells));
    uint8_t *next_cells = calloc(area, sizeof(*next_cells));
    if (cells == NULL || next_cells == NULL) {
        free(cells);
        free(next_cells);
        return false;
    }

    *grid = (LifeGrid) {
        .width = width,
        .height = height,
        .cells = cells,
        .next_cells = next_cells,
        .boundary = LIFE_BOUNDARY_DEAD
    };
    return true;
}

bool life_grid_resize(LifeGrid *grid, size_t width, size_t height)
{
    size_t area = 0;
    if (grid == NULL || grid->cells == NULL ||
        !valid_area(width, height, &area)) {
        return false;
    }

    if (width == grid->width && height == grid->height) {
        return true;
    }

    uint8_t *cells = calloc(area, sizeof(*cells));
    uint8_t *next_cells = calloc(area, sizeof(*next_cells));
    if (cells == NULL || next_cells == NULL) {
        free(cells);
        free(next_cells);
        return false;
    }

    const size_t copy_width = width < grid->width ? width : grid->width;
    const size_t copy_height = height < grid->height ? height : grid->height;
    const size_t old_x = grid->width > width ? (grid->width - width) / 2 : 0;
    const size_t old_y = grid->height > height ? (grid->height - height) / 2 : 0;
    const size_t new_x = width > grid->width ? (width - grid->width) / 2 : 0;
    const size_t new_y = height > grid->height ? (height - grid->height) / 2 : 0;

    uint64_t population = 0;
    for (size_t y = 0; y < copy_height; ++y) {
        for (size_t x = 0; x < copy_width; ++x) {
            const uint8_t age = grid->cells[(old_y + y) * grid->width + old_x + x];
            cells[(new_y + y) * width + new_x + x] = age;
            population += age != 0 ? 1U : 0U;
        }
    }

    free(grid->cells);
    free(grid->next_cells);
    grid->cells = cells;
    grid->next_cells = next_cells;
    grid->width = width;
    grid->height = height;
    grid->population = population;
    grid->births = 0;
    grid->deaths = 0;
    return true;
}

void life_grid_destroy(LifeGrid *grid)
{
    if (grid == NULL) {
        return;
    }

    free(grid->cells);
    free(grid->next_cells);
    *grid = (LifeGrid) {0};
}

void life_grid_clear(LifeGrid *grid)
{
    if (grid == NULL || grid->cells == NULL) {
        return;
    }

    const size_t area = grid->width * grid->height;
    memset(grid->cells, 0, area * sizeof(*grid->cells));
    memset(grid->next_cells, 0, area * sizeof(*grid->next_cells));
    grid->generation = 0;
    grid->population = 0;
    grid->births = 0;
    grid->deaths = 0;
}

static uint64_t next_random(uint64_t *state)
{
    uint64_t value = *state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

void life_grid_randomize(LifeGrid *grid, uint64_t seed, double density)
{
    if (grid == NULL || grid->cells == NULL) {
        return;
    }

    if (density < 0.0) {
        density = 0.0;
    } else if (density > 1.0) {
        density = 1.0;
    }

    uint64_t state = seed == 0 ? UINT64_C(0x9e3779b97f4a7c15) : seed;
    const size_t area = grid->width * grid->height;
    uint64_t population = 0;
    for (size_t i = 0; i < area; ++i) {
        const double unit = (double)(next_random(&state) >> 11) *
                            (1.0 / 9007199254740992.0);
        grid->cells[i] = unit < density ? 1U : 0U;
        population += grid->cells[i] != 0 ? 1U : 0U;
    }

    memset(grid->next_cells, 0, area * sizeof(*grid->next_cells));
    grid->generation = 0;
    grid->population = population;
    grid->births = 0;
    grid->deaths = 0;
}

static size_t wrap_coordinate(ptrdiff_t value, size_t extent)
{
    const ptrdiff_t span = (ptrdiff_t)extent;
    ptrdiff_t wrapped = value % span;
    if (wrapped < 0) {
        wrapped += span;
    }
    return (size_t)wrapped;
}

static bool sample_cell(const LifeGrid *grid, ptrdiff_t x, ptrdiff_t y)
{
    if (grid->boundary == LIFE_BOUNDARY_WRAP) {
        const size_t wrapped_x = wrap_coordinate(x, grid->width);
        const size_t wrapped_y = wrap_coordinate(y, grid->height);
        return grid->cells[cell_index(grid, wrapped_x, wrapped_y)] != 0;
    }

    if (x < 0 || y < 0 || x >= (ptrdiff_t)grid->width ||
        y >= (ptrdiff_t)grid->height) {
        return false;
    }
    return grid->cells[cell_index(grid, (size_t)x, (size_t)y)] != 0;
}

void life_grid_step(LifeGrid *grid)
{
    if (grid == NULL || grid->cells == NULL) {
        return;
    }

    uint64_t population = 0;
    uint64_t births = 0;
    uint64_t deaths = 0;

    for (size_t y = 0; y < grid->height; ++y) {
        for (size_t x = 0; x < grid->width; ++x) {
            int neighbors = 0;
            for (ptrdiff_t offset_y = -1; offset_y <= 1; ++offset_y) {
                for (ptrdiff_t offset_x = -1; offset_x <= 1; ++offset_x) {
                    if (offset_x == 0 && offset_y == 0) {
                        continue;
                    }
                    neighbors += sample_cell(grid, (ptrdiff_t)x + offset_x,
                                             (ptrdiff_t)y + offset_y) ? 1 : 0;
                }
            }

            const size_t index = cell_index(grid, x, y);
            const bool alive = grid->cells[index] != 0;
            const bool survives = alive && (neighbors == 2 || neighbors == 3);
            const bool born = !alive && neighbors == 3;

            if (survives) {
                const uint8_t age = grid->cells[index];
                grid->next_cells[index] = age == UINT8_MAX ? age : (uint8_t)(age + 1U);
                ++population;
            } else if (born) {
                grid->next_cells[index] = 1U;
                ++population;
                ++births;
            } else {
                grid->next_cells[index] = 0U;
                deaths += alive ? 1U : 0U;
            }
        }
    }

    uint8_t *old_cells = grid->cells;
    grid->cells = grid->next_cells;
    grid->next_cells = old_cells;
    ++grid->generation;
    grid->population = population;
    grid->births = births;
    grid->deaths = deaths;
}

bool life_grid_get(const LifeGrid *grid, size_t x, size_t y)
{
    return grid != NULL && grid->cells != NULL && x < grid->width &&
           y < grid->height && grid->cells[cell_index(grid, x, y)] != 0;
}

uint8_t life_grid_age(const LifeGrid *grid, size_t x, size_t y)
{
    if (grid == NULL || grid->cells == NULL || x >= grid->width ||
        y >= grid->height) {
        return 0;
    }
    return grid->cells[cell_index(grid, x, y)];
}

void life_grid_set(LifeGrid *grid, size_t x, size_t y, bool alive)
{
    if (grid == NULL || grid->cells == NULL || x >= grid->width ||
        y >= grid->height) {
        return;
    }

    const size_t index = cell_index(grid, x, y);
    const bool was_alive = grid->cells[index] != 0;
    if (was_alive == alive) {
        return;
    }

    grid->cells[index] = alive ? 1U : 0U;
    if (alive) {
        ++grid->population;
    } else {
        --grid->population;
    }
}

void life_grid_toggle(LifeGrid *grid, size_t x, size_t y)
{
    life_grid_set(grid, x, y, !life_grid_get(grid, x, y));
}

uint64_t life_grid_hash(const LifeGrid *grid)
{
    if (grid == NULL || grid->cells == NULL) {
        return 0;
    }

    uint64_t hash = UINT64_C(1469598103934665603);
    const size_t area = grid->width * grid->height;
    for (size_t i = 0; i < area; ++i) {
        hash ^= grid->cells[i] != 0 ? UINT64_C(1) : UINT64_C(0);
        hash *= UINT64_C(1099511628211);
    }
    hash ^= (uint64_t)grid->width;
    hash *= UINT64_C(1099511628211);
    hash ^= (uint64_t)grid->height;
    return hash;
}
