CC := gcc
CFLAGS := -std=c23 -Wall -Wextra -pedantic -O2
LDLIBS := -lopengl32 -lgdi32 -luser32 -lm
TARGET := opengl_shooter_3d

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.c ../modern_gl_compat.h Makefile
	$(CC) $(CFLAGS) -mwindows -o $@ $< $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
