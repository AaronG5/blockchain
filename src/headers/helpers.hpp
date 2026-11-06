#pragma once

#include <atomic>
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

constexpr std::size_t CANDIDATES_PER_BLOCK = 5;
constexpr std::uint64_t FIRST_ROUND_BUDGET = 1000;
constexpr std::uint64_t MAX_ATTEMPTS_PER_BLOCK = 500000;

struct Candidate {
   Block block;
   std::uint64_t nextNonce = 0;
   std::uint64_t attempts = 0;
   double timeSec = 0;
   bool solved = false;
};

struct RaceResult {
   int winner = -1;
   int rounds = 0;   // stays 0 in parallel mode, which has no rounds
   std::uint64_t totalAttempts = 0;
   double timeSec = 0;
};

std::vector<Candidate> buildCandidates(std::vector<Transaction>& pool, const Blockchain& chain, Random& rng, std::size_t count);

bool mineWithBudget(Candidate& candidate, std::uint64_t budget, const std::atomic<bool>* stop = nullptr);

RaceResult mineCandidates(std::vector<Candidate>& candidates, std::uint64_t budget, std::uint64_t maxTotalAttempts);

// One thread per candidate, no rounds: mines until one finds a nonce (and stops the others) or its share of the limit runs out
RaceResult mineCandidatesParallel(std::vector<Candidate>& candidates, std::uint64_t maxTotalAttempts);

std::string findPermanentProblem(const Transaction& tx, const Blockchain& chain);

std::vector<Transaction> filterPool(const std::vector<Transaction>& generated, const Blockchain& chain, std::vector<Rejected>& rejected);

void removeFromPool(std::vector<Transaction>& pool, const std::vector<Transaction>& included);

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng);

void saveBalances(const std::string& path, const std::vector<User>& users, const State& state);