CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -pedantic -O2
LDLIBS := -lopengl32 -lgdi32 -luser32 -lm
TARGET := opengl_shooter_3d

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -mwindows -o $@ $< $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
