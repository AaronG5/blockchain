CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-function -Wno-sign-compare -Isrc/headers -pthread

TARGET := bin/blockchain
SOURCES := main.cpp $(wildcard src/functions/*.cpp)
OBJECTS := $(SOURCES:%.cpp=bin/obj/%.o)

# Tests use everything except main.cpp
TEST_TARGET := bin/tests
TEST_OBJECTS := bin/obj/tests/tests.o $(filter-out bin/obj/main.o,$(OBJECTS))

DEPENDENCIES := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

# make run ARGS="7 5 8 1"  ->  [seed] [users] [transactions] [bad transactions per kind]
ARGS ?=
OUTPUT := output.txt

# bash for `set -o pipefail` in the run target
SHELL := /bin/bash

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# -MMD records which headers each file includes, so changing a header rebuilds the right files
bin/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# tee prints the output and writes it to output.txt, replacing what was there.
# pipefail keeps the program's exit code, otherwise make would only see tee's
run: $(TARGET)
	set -o pipefail; ./$(TARGET) $(ARGS) | tee $(OUTPUT)

# Same ARGS as run, 5 sequential and 5 parallel runs, results in benchmark.csv
benchmark: $(TARGET)
	./$(TARGET) $(ARGS) --benchmark

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf bin

-include $(DEPENDENCIES)

.PHONY: all run benchmark test clean
