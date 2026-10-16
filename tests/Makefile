CC := gcc
CXX := g++
CFLAGS := -std=c23 -O2 -Wall -Wextra -Wpedantic -MMD -MP
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -MMD -MP
LDLIBS := -lgdi32 -luser32 -lopengl32 -lm
TESTS := fluid_input ray_geometry pendulum shooter_input shooter3d_input platform_input audio conway_input
TARGETS := $(addprefix build/test_,$(addsuffix .exe,$(TESTS)))

.PHONY: check check-graphics clean

check: $(TARGETS)
	@set -e; for test in $(TARGETS); do echo "Running $$test"; ./$$test; done

build/test_%.exe: test_%.c test.h Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -I../fluid-sim-3d-c/include -o $@ $< $(LDLIBS)

build/test_audio.exe: test_audio.cpp test.h Makefile
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -I../keyboard-piano-cpp/include -o $@ $< -lole32 -luser32 -lavrt

build/test_conway_input.exe: test_conway_input.c test.h Makefile ../conways-game-of-life-c/src/main.c \
	../conways-game-of-life-c/src/life.c ../conways-game-of-life-c/src/patterns.c \
	../conways-game-of-life-c/src/terminal.c $(wildcard ../conways-game-of-life-c/include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -I../conways-game-of-life-c/include \
		-o $@ $(filter-out ../conways-game-of-life-c/src/main.c,$(filter %.c,$^))

check-graphics: build/test_graphics.exe
	./build/test_graphics.exe

clean:
	rm -f $(TARGETS) $(TARGETS:.exe=.d) build/test_graphics.exe build/test_graphics.d

-include $(TARGETS:.exe=.d) build/test_graphics.d
