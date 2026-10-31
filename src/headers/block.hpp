#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <utility>

#include "transaction.hpp"
#include "hash.hpp"

class Block {
public:
   struct Header {
      std::string prevHash;
      std::int64_t timestamp;
      std::uint32_t version;
      std::string txRoot;
      std::uint32_t difficulty;
      std::uint64_t nonce;

      std::string serialize() const {
         return prevHash + "|" + std::to_string(timestamp) + "|" 
                + std::to_string(version) + "|" + txRoot + "|" 
                + std::to_string(difficulty) + "|" + std::to_string(nonce);
      }
   };

   Block(std::string prevHash, std::int64_t timestamp, std::uint32_t version, 
         std::vector<Transaction> transactions, std::uint32_t difficulty) 
   : transactions(std::move(transactions)) {
      header.prevHash = std::move(prevHash);
      header.timestamp = timestamp;
      header.version = version;
      header.txRoot = computeTxRoot(this->transactions);
      header.difficulty = difficulty;
      header.nonce = 0;
   }
      
   std::string hash() const { 
      return hash::hashFunction(header.serialize()); 
   }

   void setNonce(const std::uint64_t& nonce) { header.nonce = nonce; }

   const Header& getHeader() const { return header; }
   const std::vector<Transaction>& getTransactions() const { return transactions; }
      
private:
   Header header;
   std::vector<Transaction> transactions;

   std::string computeTxRoot(const std::vector<Transaction>& transactions) const {
      std::string acc = "";
      for(auto& tx : transactions) {
         if(!acc.empty()) acc += "|";
         acc += tx.getId(); // Serialize for merkle tree
      }
      return hash::hashFunction(acc);
   }
};