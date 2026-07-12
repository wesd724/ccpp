CXX := g++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -MMD -MP
LDLIBS := -lole32 -luser32 -lavrt

TARGET := keyboard_piano
SOURCES := src/main.cpp src/audio_engine.cpp src/keyboard.cpp
OBJECTS := $(patsubst src/%.cpp,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run check clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

check: $(TARGET)
	./$(TARGET) --check-audio

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJECTS) $(DEPS)

-include $(DEPS)
