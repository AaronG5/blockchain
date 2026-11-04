#pragma once

#include <vector>
#include <chrono>

#include "transaction.hpp"
#include "block.hpp"

struct MiningResult {
   std::uint64_t attempts;
   double timeSec;
};

MiningResult mine(Block& block);