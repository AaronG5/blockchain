#include "helpers.hpp"

#include <iostream>

#include "blockchain.hpp"

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

std::vector<Transaction> selectTransactions(std::vector<Transaction>& pool, const State& confirmed, Random& rng) {
   std::vector<Transaction> selected;
   std::string reason;
   State temp = confirmed;
   rng.shuffle(pool);

   for(int i = 0; i < MAX_TRANSACTIONS_PER_BLOCK; ++i) {
      const auto& tx = pool[i];
      if(temp.apply(tx, reason)) {
         selected.push_back(tx);
      }
      else { std::cout << reason << std::endl; }
   }

   return selected;
}