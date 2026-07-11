CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -O2
TARGET := rotating_cube_cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
