CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -pedantic -O2
LDFLAGS := -lm
TARGET := rotating_cube

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
