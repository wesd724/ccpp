CC := gcc
CPPFLAGS := -Iinclude
CFLAGS := -std=c23 -O3 -Wall -Wextra -MMD -MP -fopenmp
LDLIBS := -fopenmp -lopengl32 -lgdi32 -luser32
TARGET := fluid_sim_3d.exe

SOURCES := main.c $(wildcard src/*.c)
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build $(TARGET)

-include $(DEPS)
