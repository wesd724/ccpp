#include "test.h"
#include "../double-pendulum-c/main.c"

static double energy(void)
{
    return 0.5 * (mass1 + mass2) * length1 * length1 * state.omega1 * state.omega1
        + 0.5 * mass2 * length2 * length2 * state.omega2 * state.omega2
        + mass2 * length1 * length2 * state.omega1 * state.omega2 * cos(state.theta1 - state.theta2)
        - (mass1 + mass2) * gravity * length1 * cos(state.theta1)
        - mass2 * gravity * length2 * cos(state.theta2);
}

int main(void)
{
    reset_simulation();
    const double initial = energy();
    for (int i = 0; i < 10000; ++i) integrate();
    CHECK(isfinite(energy()));
    CHECK(fabs(energy() - initial) < 0.01);
    reset_simulation();
    for (int i = 0; i < 100; ++i) advance_simulation(0.01);
    const State timed = state;
    reset_simulation();
    for (int i = 0; i < 166; ++i) integrate();
    CHECK(fabs(timed.theta1 - state.theta1) < 0.0000001);
    CHECK(fabs(timed.theta2 - state.theta2) < 0.0000001);
    paused = true;
    const State before_pause = state;
    CHECK(!advance_simulation(10.0));
    CHECK(state.theta1 == before_pause.theta1 && accumulator == 0);
    paused = false;
    state = (State){0};
    integrate();
    CHECK(state.theta1 == 0 && state.theta2 == 0);
    CHECK(state.omega1 == 0 && state.omega2 == 0);
    puts("Pendulum RK4 energy, real-time stepping, pause and equilibrium passed.");
    return 0;
}
