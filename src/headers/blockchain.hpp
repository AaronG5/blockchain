#pragma once

#include <vector>
#include <unordered_set>
#include <string>
#include <ctime>

#include "block.hpp"
#include "state.hpp"

#define DIFFICULTY 3

constexpr size_t MAX_TRANSACTIONS_PER_BLOCK = 100;

class Blockchain {
private:
   std::vector<Block> chain;
   State initial;
   State confirmed;
   std::unordered_set<std::string> includedIds;
   std::unordered_set<std::string> knownKeys;
   unsigned int difficulty;
   unsigned int version;

   bool verifyBlock(const Block& block, State& state, std::string& reason) const {
      const Block::Header& header = block.getHeader();
      const std::vector<Transaction>& transactions = block.getTransactions();

      if(header.version != version) { reason = "wrong version"; return false; }
      if(header.prevHash != getHeadHash()) { reason = "prevHash does not match chain head"; return false; }
      if(header.difficulty != difficulty) { reason = "wrong difficulty"; return false; }
      if(!block.hasValidPow()) { reason = "hash does not meet difficulty"; return false; }
      if(transactions.size() > MAX_TRANSACTIONS_PER_BLOCK) { reason = "too many transactions"; return false; }
      if(header.txRoot != Block::computeTxRoot(transactions)) { reason = "txRoot mismatch"; return false; }

      std::unordered_set<std::string> seen;
      for(const Transaction& tx : transactions) {
         if(!tx.hasValidId()) { reason = "invalid id " + tx.getId(); return false; }
         if(includedIds.count(tx.getId())) { reason = "already in chain " + tx.getId(); return false; }
         if(!seen.insert(tx.getId()).second) { reason = "duplicate in block " + tx.getId(); return false; }
         for(const Output& out : tx.getOutputs()) {
            if(!isKnownUser(out.ownerPK)) { reason = "unknown receiver " + tx.getId(); return false; }
         }
         if(!state.apply(tx, reason)) return false;
      }
      return true;
   }

public:
   Blockchain(State initialState, std::unordered_set<std::string> knownKeys, unsigned int difficulty = DIFFICULTY, unsigned int version = 2)
      : initial(std::move(initialState)), confirmed(initial), knownKeys(std::move(knownKeys)), difficulty(difficulty), version(version) {}

   Block createBlock(const std::vector<Transaction>& transactions) const {
      return Block(getHeadHash(), std::time(nullptr), version, transactions, difficulty);
   }

   bool addBlock(const Block& block, std::string& reason) {
      State temp = confirmed;
      if(!verifyBlock(block, temp, reason)) return false;

      confirmed = std::move(temp);
      for(const Transaction& tx : block.getTransactions()) includedIds.insert(tx.getId());
      chain.push_back(block);
      return true;
   }

   std::string getHeadHash() const {
      return chain.empty() ? std::string(64, '0') : chain.back().hash();
   }

   bool isKnownUser(const std::string& publicKey) const { return knownKeys.count(publicKey) > 0; }
   bool isIncluded(const std::string& transactionId) const { return includedIds.count(transactionId) > 0; }

   int size() const { return chain.size(); }
   const std::vector<Block>& getChain() const { return chain; }
   const State& getInitial() const { return initial; }
   const State& getConfirmed() const { return confirmed; }
   int getDifficulty() const { return difficulty; }
   int getVersion() const { return version; }
};