#pragma once

#include <vector>
#include <chrono>
#include <string>

#include "transaction.hpp"
#include "block.hpp"
#include "state.hpp"
#include "generator.hpp"
#include "user.hpp"


struct MiningResult {
   std::uint64_t attempts;
   double timeSec;
};

MiningResult mine(Block& block);

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng);

// Writes username,public_key,amount for every user, in the order of `users`
void saveBalances(const std::string& path, const std::vector<User>& users, const State& state);