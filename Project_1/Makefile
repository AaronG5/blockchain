CXX = g++
CXXFLAGS = -std=c++17 -O2
TARGET = hasher
SOURCES = main.cpp
HEADERS = hash.hpp stb_image.h

.PHONY: build rebuild clean

build: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

rebuild: clean build

clean:
	rm -f $(TARGET)