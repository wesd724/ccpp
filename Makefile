PROJECTS := rotating-cube-c rotating-cube-cpp double-pendulum-c fluid-sim-c \
	ray-tracer-c opengl-shooter-c opengl-shooter-3d-c fluid-sim-3d-c \
	keyboard-piano-cpp conways-game-of-life-c

.PHONY: all check check-audio check-graphics $(PROJECTS)

all: $(PROJECTS)

$(PROJECTS):
	$(MAKE) -C $@

check:
	$(MAKE) -C tests check
	$(MAKE) -C conways-game-of-life-c check

check-audio:
	$(MAKE) -C keyboard-piano-cpp check

check-graphics:
	$(MAKE) -C tests check-graphics
