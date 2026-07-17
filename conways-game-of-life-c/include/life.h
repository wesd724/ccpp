#ifndef CONWAYS_LIFE_H
#define CONWAYS_LIFE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum LifeBoundary {
    LIFE_BOUNDARY_DEAD,
    LIFE_BOUNDARY_WRAP
} LifeBoundary;

typedef struct LifeGrid {
    size_t width;
    size_t height;
    uint8_t *cells;
    uint8_t *next_cells;
    uint64_t generation;
    uint64_t population;
    uint64_t births;
    uint64_t deaths;
    LifeBoundary boundary;
} LifeGrid;

bool life_grid_init(LifeGrid *grid, size_t width, size_t height);
bool life_grid_resize(LifeGrid *grid, size_t width, size_t height);
void life_grid_destroy(LifeGrid *grid);
void life_grid_clear(LifeGrid *grid);
void life_grid_randomize(LifeGrid *grid, uint64_t seed, double density);
void life_grid_step(LifeGrid *grid);

bool life_grid_get(const LifeGrid *grid, size_t x, size_t y);
uint8_t life_grid_age(const LifeGrid *grid, size_t x, size_t y);
void life_grid_set(LifeGrid *grid, size_t x, size_t y, bool alive);
void life_grid_toggle(LifeGrid *grid, size_t x, size_t y);
uint64_t life_grid_hash(const LifeGrid *grid);

#endif
