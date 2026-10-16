CC := gcc
CFLAGS := -std=c23 -Wall -Wextra -pedantic -O2
LDFLAGS := -lm
TARGET := rotating_cube

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.c Makefile
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
