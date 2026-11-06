#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

#include "../headers/helpers.hpp"
#include "../headers/blockchain.hpp"

MiningResult mine(Block& block) {
   auto start = std::chrono::steady_clock::now();

   std::uint64_t nonce = 0;
   block.setNonce(nonce);
   while(!block.hasValidPow()) {
      block.setNonce(++nonce);
   }

   auto end = std::chrono::steady_clock::now();
   double seconds = std::chrono::duration<double>(end - start).count();

   return {nonce + 1, seconds};
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
   std::string reason;
   State temp = confirmed;
   rng.shuffle(pool);

   for(const Transaction& tx : pool) {
      if(selected.size() == MAX_TRANSACTIONS_PER_BLOCK) break;
      if(temp.apply(tx, reason)) selected.push_back(tx);
      // else { std::cout << reason << std::endl; }
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