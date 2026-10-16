CC := gcc
CPPFLAGS := -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS := -std=c23 -O2 -Wall -Wextra -Wpedantic -Wshadow -MMD -MP
TARGET := conways_life

SOURCES := src/main.c src/life.c src/patterns.c src/terminal.c
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
TEST_TARGET := build/test_life

.PHONY: all run check clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

build/%.o: src/%.c Makefile
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TEST_TARGET): tests/test_life.c src/life.c src/patterns.c include/life.h include/patterns.h Makefile
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)

run: $(TARGET)
	./$(TARGET)

check: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf build $(TARGET) $(TARGET).exe

-include $(DEPS)
