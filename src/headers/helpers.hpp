#pragma once

#include <vector>
#include <chrono>

#include "transaction.hpp"
#include "block.hpp"
#include "state.hpp"
#include "generator.hpp"


struct MiningResult {
   std::uint64_t attempts;
   double timeSec;
};

MiningResult mine(Block& block);

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng);