CXX = g++
CXXFLAGS = -std=c++17 -O3
CLI_TARGET = cli_hasher
CLI_SOURCE = cli.cpp
CLI_V2_TARGET = cli_hasher_v2
EXP1_DIR = Experiment/exp1
BENCH_KONST_TARGET = $(EXP1_DIR)/bench_konst
BENCH_KONST_SOURCE = $(EXP1_DIR)/bench_konst.cpp
EXP2_DIR = Experiment/exp2
COLLISION_TARGET = $(EXP2_DIR)/collision_test
COLLISION_SOURCE = $(EXP2_DIR)/collision_test.cpp
AVALANCHE_TARGET = $(EXP2_DIR)/avalanche_test
AVALANCHE_SOURCE = $(EXP2_DIR)/avalanche_test.cpp
EXP3_DIR = Experiment/exp3
BRUTE_FORCE_TARGET = $(EXP3_DIR)/brute_force
BRUTE_FORCE_SOURCE = $(EXP3_DIR)/brute_force.cpp
EXP4_DIR = Experiment/exp4
COMPARE_TARGET = $(EXP4_DIR)/compare
COMPARE_SOURCE = $(EXP4_DIR)/compare.cpp
TARGETS = $(CLI_TARGET) $(CLI_V2_TARGET) $(BENCH_KONST_TARGET) $(COLLISION_TARGET) $(AVALANCHE_TARGET) $(BRUTE_FORCE_TARGET) $(COMPARE_TARGET)
HEADERS = hash.hpp stb_image.h
HEADERS_V2 = hash_v2.hpp stb_image.h

.PHONY: build rebuild clean

build: build_cli build_cli_v2 build_bench_konst build_collision_test build_avalanche_test build_brute_force build_compare

build_cli: $(CLI_TARGET)

$(CLI_TARGET): $(CLI_SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CLI_SOURCE) -o $(CLI_TARGET)

build_cli_v2: $(CLI_V2_TARGET)

$(CLI_V2_TARGET): $(CLI_SOURCE) $(HEADERS_V2)
	$(CXX) $(CXXFLAGS) -DUSE_HASH_V2 $(CLI_SOURCE) -o $(CLI_V2_TARGET)

build_bench_konst: $(BENCH_KONST_TARGET) 

$(BENCH_KONST_TARGET): $(BENCH_KONST_SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(BENCH_KONST_SOURCE) -o $(BENCH_KONST_TARGET)

build_collision_test: $(COLLISION_TARGET)

$(COLLISION_TARGET): $(COLLISION_SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(COLLISION_SOURCE) -o $(COLLISION_TARGET)

build_avalanche_test: $(AVALANCHE_TARGET)

$(AVALANCHE_TARGET): $(AVALANCHE_SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(AVALANCHE_SOURCE) -o $(AVALANCHE_TARGET)

build_brute_force: $(BRUTE_FORCE_TARGET)

$(BRUTE_FORCE_TARGET): $(BRUTE_FORCE_SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(BRUTE_FORCE_SOURCE) -o $(BRUTE_FORCE_TARGET)

build_compare: $(COMPARE_TARGET)

$(COMPARE_TARGET): $(COMPARE_SOURCE) $(HEADERS) $(HEADERS_V2)
	$(CXX) $(CXXFLAGS) $(COMPARE_SOURCE) -o $(COMPARE_TARGET)

rebuild: clean build

clean:
	rm -f $(TARGETS)