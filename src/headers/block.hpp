#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "transaction.hpp"

class Block {
public:
   struct Header {
      std::string prevHash;
      std::int64_t timestamp;
      std::uint32_t version;
      std::string txRoot;
      std::uint32_t difficulty;
      std::uint64_t nonce;
   };

private:
   Header header;
   std::vector<Transaction> transactions;
   
public:
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

   std::string serialize() const {
      return header.prevHash + "|" + std::to_string(header.timestamp) + "|" 
             + std::to_string(header.version) + "|" + header.txRoot + "|" 
             + std::to_string(header.nonce) + "|" + std::to_string(header.nonce);
   }

private:
      std::string computeTxRoot(std::vector<Transaction> transactions) const {}
};