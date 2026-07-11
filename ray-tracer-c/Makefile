CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -pedantic -O2
LDLIBS := -lgdi32 -luser32 -lm
TARGET := ray_tracer

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -mwindows -o $@ $< $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe ray_tracer_output.ppm
