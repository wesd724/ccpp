#include "life.h"
#include "patterns.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int tests_run = 0;

#define EXPECT(condition)                                                        \
    do {                                                                         \
        if (!(condition)) {                                                       \
            fprintf(stderr, "%s:%d: expectation failed: %s\n",                  \
                    __FILE__, __LINE__, #condition);                              \
            ++failures;                                                          \
        }                                                                        \
    } while (false)

static void begin_test(void)
{
    ++tests_run;
}

static void test_block_is_stable(void)
{
    begin_test();
    LifeGrid grid = {0};
    EXPECT(life_grid_init(&grid, 6, 6));
    life_pattern_place(&grid, life_pattern_get(0), 3, 3);
    const uint64_t before = life_grid_hash(&grid);
    life_grid_step(&grid);
    EXPECT(grid.population == 4);
    EXPECT(grid.births == 0);
    EXPECT(grid.deaths == 0);
    EXPECT(life_grid_hash(&grid) == before);
    life_grid_destroy(&grid);
}

static void test_blinker_oscillates(void)
{
    begin_test();
    LifeGrid grid = {0};
    EXPECT(life_grid_init(&grid, 7, 7));
    life_pattern_place(&grid, life_pattern_get(1), 3, 3);
    const uint64_t horizontal = life_grid_hash(&grid);

    life_grid_step(&grid);
    EXPECT(grid.population == 3);
    EXPECT(grid.births == 2);
    EXPECT(grid.deaths == 2);
    EXPECT(life_grid_get(&grid, 3, 2));
    EXPECT(life_grid_get(&grid, 3, 3));
    EXPECT(life_grid_get(&grid, 3, 4));

    life_grid_step(&grid);
    EXPECT(grid.generation == 2);
    EXPECT(life_grid_hash(&grid) == horizontal);
    life_grid_destroy(&grid);
}

static void test_underpopulation_and_overpopulation(void)
{
    begin_test();
    LifeGrid grid = {0};
    EXPECT(life_grid_init(&grid, 5, 5));
    life_grid_set(&grid, 1, 1, true);
    life_grid_set(&grid, 2, 1, true);
    life_grid_set(&grid, 1, 2, true);
    life_grid_set(&grid, 2, 2, true);
    life_grid_set(&grid, 3, 2, true);
    life_grid_step(&grid);
    EXPECT(!life_grid_get(&grid, 2, 1));
    EXPECT(!life_grid_get(&grid, 2, 2));
    life_grid_destroy(&grid);
}

static void test_wrapped_boundary(void)
{
    begin_test();
    LifeGrid dead = {0};
    LifeGrid wrapped = {0};
    EXPECT(life_grid_init(&dead, 5, 5));
    EXPECT(life_grid_init(&wrapped, 5, 5));
    wrapped.boundary = LIFE_BOUNDARY_WRAP;

    const size_t positions[][2] = {{4, 4}, {0, 4}, {1, 4}};
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
        life_grid_set(&dead, positions[i][0], positions[i][1], true);
        life_grid_set(&wrapped, positions[i][0], positions[i][1], true);
    }

    life_grid_step(&dead);
    EXPECT(dead.population == 0);

    life_grid_step(&wrapped);
    EXPECT(wrapped.population == 3);
    EXPECT(life_grid_get(&wrapped, 0, 3));
    EXPECT(life_grid_get(&wrapped, 0, 4));
    EXPECT(life_grid_get(&wrapped, 0, 0));
    life_grid_destroy(&dead);
    life_grid_destroy(&wrapped);
}

static void test_patterns_have_expected_population(void)
{
    begin_test();
    const size_t expected[] = {4, 3, 5, 48, 36};
    EXPECT(life_pattern_count() == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < life_pattern_count(); ++i) {
        const LifePattern *pattern = life_pattern_get(i);
        EXPECT(strlen(pattern->cells) == pattern->width * pattern->height);
        EXPECT(life_pattern_live_count(pattern) == expected[i]);
    }
}

static void test_randomization_is_reproducible(void)
{
    begin_test();
    LifeGrid first = {0};
    LifeGrid second = {0};
    EXPECT(life_grid_init(&first, 20, 10));
    EXPECT(life_grid_init(&second, 20, 10));
    life_grid_randomize(&first, 12345, 0.25);
    life_grid_randomize(&second, 12345, 0.25);
    EXPECT(first.population == second.population);
    EXPECT(life_grid_hash(&first) == life_grid_hash(&second));
    life_grid_destroy(&first);
    life_grid_destroy(&second);
}

static void test_resize_preserves_center(void)
{
    begin_test();
    LifeGrid grid = {0};
    EXPECT(life_grid_init(&grid, 5, 5));
    life_grid_set(&grid, 2, 2, true);
    EXPECT(life_grid_resize(&grid, 9, 7));
    EXPECT(grid.population == 1);
    EXPECT(life_grid_get(&grid, 4, 3));
    EXPECT(life_grid_resize(&grid, 3, 3));
    EXPECT(grid.population == 1);
    EXPECT(life_grid_get(&grid, 1, 1));
    life_grid_destroy(&grid);
}

int main(void)
{
    test_block_is_stable();
    test_blinker_oscillates();
    test_underpopulation_and_overpopulation();
    test_wrapped_boundary();
    test_patterns_have_expected_population();
    test_randomization_is_reproducible();
    test_resize_preserves_center();

    if (failures != 0) {
        fprintf(stderr, "%d of %d tests failed.\n", failures, tests_run);
        return EXIT_FAILURE;
    }
    printf("All %d Conway Life tests passed.\n", tests_run);
    return EXIT_SUCCESS;
}
