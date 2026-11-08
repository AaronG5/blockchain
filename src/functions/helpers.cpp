#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <unordered_set>

#include "../headers/helpers.hpp"
#include "../headers/blockchain.hpp"

MiningResult mine(Block& block) {
   Candidate candidate{std::move(block)};
   mineWithBudget(candidate, std::numeric_limits<std::uint64_t>::max());
   block = std::move(candidate.block);
   return {candidate.attempts, candidate.timeSec};
}

std::vector<Candidate> buildCandidates(std::vector<Transaction>& pool, const Blockchain& chain, Random& rng, std::size_t count) {
   std::vector<Candidate> candidates;
   for(std::size_t i = 0; i < count; ++i) {
      std::vector<Transaction> selected = selectTransactions(pool, chain.getConfirmed(), rng);
      if(selected.empty()) break;
      candidates.push_back({chain.createBlock(selected)});
   }
   return candidates;
}

// Only the nonce changes between attempts, so the rest of the header is serialized once and every attempt just
// writes the nonce digits after it, in the same buffer, without allocating. The progress is kept in local
// variables and written to the candidate at the end, so parallel threads don't keep writing next to each other
bool mineWithBudget(Candidate& candidate, std::uint64_t budget, const std::atomic<bool>* stop) {
   const auto start = std::chrono::steady_clock::now();

   const std::uint32_t difficulty = candidate.block.getHeader().difficulty;
   std::string serialized = candidate.block.getHeader().serializePrefix();
   const std::size_t prefixSize = serialized.size();
   char digits[std::numeric_limits<std::uint64_t>::digits10 + 1];
   serialized.reserve(prefixSize + sizeof(digits));

   bool solved = candidate.solved;
   std::uint64_t nonce = candidate.nextNonce;
   std::uint64_t attempts = 0;
   while(!solved && attempts < budget && !(stop && stop->load(std::memory_order_relaxed))) {
      const char* end = std::to_chars(digits, digits + sizeof(digits), nonce).ptr;
      serialized.resize(prefixSize);
      serialized.append(digits, end - digits);

      ++attempts;
      if(Block::meetsDifficulty(hash::hashFunction(serialized), difficulty)) {
         candidate.block.setNonce(nonce);
         solved = true;
      }
      ++nonce;
   }

   candidate.nextNonce = nonce;
   candidate.attempts += attempts;
   candidate.solved = solved;

   const auto end = std::chrono::steady_clock::now();
   candidate.timeSec += std::chrono::duration<double>(end - start).count();
   return solved;
}

RaceResult mineCandidates(std::vector<Candidate>& candidates, std::uint64_t budget, std::uint64_t maxTotalAttempts) {
   const auto start = std::chrono::steady_clock::now();
   RaceResult result;

   while(!candidates.empty() && result.winner < 0 && result.totalAttempts < maxTotalAttempts) {
      ++result.rounds;
      for(int i = 0; i < candidates.size(); ++i) {
         const std::uint64_t allowed = std::min(budget, maxTotalAttempts - result.totalAttempts);
         if(allowed == 0) break;

         const std::uint64_t before = candidates[i].attempts;
         const bool found = mineWithBudget(candidates[i], allowed);
         result.totalAttempts += candidates[i].attempts - before;

         if(found) {
            result.winner = i;
            break;
         }
      }
      budget *= 2;
   }

   const auto end = std::chrono::steady_clock::now();
   result.timeSec = std::chrono::duration<double>(end - start).count();
   return result;
}

RaceResult mineCandidatesParallel(std::vector<Candidate>& candidates, std::uint64_t maxTotalAttempts) {
   const auto start = std::chrono::steady_clock::now();
   RaceResult result;
   if(candidates.empty()) return result;

   // No rounds: every thread keeps mining until someone wins or its equal share of the limit runs out
   const std::uint64_t share = maxTotalAttempts / candidates.size();
   std::atomic<int> winner{-1};
   std::atomic<bool> stop{false};
   std::vector<std::uint64_t> before(candidates.size());
   std::vector<std::thread> workers;

   for(int i = 0; i < candidates.size(); ++i) {
      before[i] = candidates[i].attempts;
      workers.emplace_back([&candidates, &winner, &stop, share, i] {
         if(!mineWithBudget(candidates[i], share, &stop)) return;

         // Only the first thread to get here becomes the winner, even if two find a nonce at the same time
         int expected = -1;
         if(winner.compare_exchange_strong(expected, i)) stop = true;
      });
   }
   for(std::thread& worker : workers) worker.join();

   for(int i = 0; i < candidates.size(); ++i) {
      result.totalAttempts += candidates[i].attempts - before[i];
   }
   result.winner = winner.load();

   const auto end = std::chrono::steady_clock::now();
   result.timeSec = std::chrono::duration<double>(end - start).count();
   return result;
}

std::string findPermanentProblem(const Transaction& tx, const Blockchain& chain) {
   if(!tx.hasValidId()) return "invalid id";
   if(tx.getInputs().empty()) return "missing inputs";
   if(tx.getOutputs().empty()) return "missing outputs";

   for(const Output& out : tx.getOutputs()) {
      if(out.amount == 0) return "zero amount output";
      if(!chain.isKnownUser(out.ownerPK)) return "unknown receiver";
   }

   if(chain.isIncluded(tx.getId())) return "id already in chain";
   return "";
}

std::vector<Transaction> filterPool(const std::vector<Transaction>& generated, const Blockchain& chain, std::vector<Rejected>& rejected) {
   std::vector<Transaction> pool;
   pool.reserve(generated.size());

   std::unordered_set<std::string> poolIds;
   for(const Transaction& tx : generated) {
      std::string problem = findPermanentProblem(tx, chain);
      if(problem.empty() && poolIds.count(tx.getId())) problem = "id already in pool";

      if(problem.empty()) {
         poolIds.insert(tx.getId());
         pool.push_back(tx);
      }
      else {
         rejected.push_back({tx, problem});
      }
   }
   return pool;
}

void removeFromPool(std::vector<Transaction>& pool, const std::vector<Transaction>& included) {
   std::unordered_set<std::string> includedIds;
   for(const Transaction& tx : included) includedIds.insert(tx.getId());

   pool.erase(std::remove_if(pool.begin(), pool.end(), [&includedIds](const Transaction& tx) {
      return includedIds.count(tx.getId()) > 0;
   }), pool.end());
}

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng) {
   std::vector<Transaction> selected;
   State temp = confirmed;
   rng.shuffle(pool);

   for(const Transaction& tx : pool) {
      if(selected.size() == MAX_TRANSACTIONS_PER_BLOCK) break;
      if(temp.tryApply(tx)) selected.push_back(tx);
   }

   return selected;
}

void saveBalances(const std::string& path, const std::vector<User>& users, const State& state) {
   std::ofstream file(path);
   if(!file) {
      throw std::runtime_error("Could not open " + path);
   }

   file << "username,public_key,amount\n";
   for(const User& user : users) {
      file << user.getUserName() << "," << user.getPublicKey() << "," << state.balanceOf(user.getPublicKey()) << "\n";
   }
}