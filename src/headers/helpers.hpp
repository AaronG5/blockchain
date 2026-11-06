#pragma once

#include <vector>
#include <chrono>
#include <string>

#include "transaction.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "state.hpp"
#include "generator.hpp"
#include "user.hpp"


struct MiningResult {
   std::uint64_t attempts;
   double timeSec;
};

struct Rejected {
   Transaction tx;
   std::string reason;
};

MiningResult mine(Block& block);

// Pool entry: problems that no future state can fix. Returns "" when there are none
std::string findPermanentProblem(const Transaction& tx, const Blockchain& chain);

// Keeps the transactions that pass pool entry, the rest go to `rejected` with a reason
std::vector<Transaction> filterPool(const std::vector<Transaction>& generated, const Blockchain& chain, std::vector<Rejected>& rejected);

void removeFromPool(std::vector<Transaction>& pool, const std::vector<Transaction>& included);

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng);

// Writes username,public_key,amount for every user, in the order of `users`
void saveBalances(const std::string& path, const std::vector<User>& users, const State& state);