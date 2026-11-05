CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-function -Wno-sign-compare -Isrc/headers

TARGET := bin/blockchain
SOURCES := main.cpp $(wildcard src/functions/*.cpp)
OBJECTS := $(SOURCES:%.cpp=bin/obj/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

# make run ARGS="7 5 8 1"  ->  [seed] [users] [transactions] [bad transactions per kind]
ARGS ?=

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# -MMD records which headers each file includes, so changing a header rebuilds the right files
bin/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -rf bin

-include $(DEPENDENCIES)

.PHONY: all run clean
