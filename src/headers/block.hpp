#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <stdexcept>

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
      : transactions(std::move(transactions)) 
   {
      header.prevHash = std::move(prevHash);
      header.timestamp = timestamp;
      header.version = version;
      header.txRoot = computeTxRoot(this->transactions);
      header.difficulty = difficulty;
      header.nonce = 0;
   }

   Block(Header header, std::vector<Transaction> transactions) 
      : header(std::move(header)), transactions(std::move(transactions)) {}
      
   std::string hash() const { 
      return hash::hashFunction(header.serialize()); 
   }

   static std::string computeTxRoot(const std::vector<std::string>& txIds) {
      if(txIds.empty()) return hash::hashFunction("");
      if(txIds.size() == 1) return txIds.front();
      std::vector<std::string> hashes;
      for(int i = 0; i < txIds.size(); i += 2) {
         std::string node1 = hexToBytes(txIds[i]);
         std::string node2 = hexToBytes((i + 1 < txIds.size()) ? txIds[i + 1] : txIds[i]);
         hashes.push_back(hash::hashFunction(node1 + node2));
      }

      return computeTxRoot(hashes);
   }

   static std::string hexToBytes(const std::string& hexStr) {
      if(hexStr.size() % 2 == 1) throw std::invalid_argument("hex input size can't be odd when decoding to bytes");
      std::string byteStr;
      for(int i = 0; i < hexStr.size(); i += 2) {
         char a = hexCharToByte(hexStr[i]);
         char b = hexCharToByte(hexStr[i+1]);
         byteStr += a * 16 + b;
      }
      return byteStr;
   }

   static char hexCharToByte(char c) {
      if(c >= '0' && c <= '9') return c - '0';
      if(c >= 'a' && c <= 'f') return c - 'a' + 10;
      throw std::invalid_argument("char isn't a hex value");
   }

   static std::string computeTxRoot(const std::vector<Transaction>& transactions) {
      std::vector<std::string> txIds;
      for(const auto& tx : transactions) txIds.push_back(tx.getId());
      return computeTxRoot(txIds);
   }

   bool hasValidPow() const {
      const std::string h = hash();
      return h.compare(0, header.difficulty, std::string(header.difficulty, '0')) == 0;
   }

   void setNonce(std::uint64_t nonce) { header.nonce = nonce; }

   const Header& getHeader() const { return header; }
   const std::vector<Transaction>& getTransactions() const { return transactions; }

private:
   Header header;
   std::vector<Transaction> transactions;
};