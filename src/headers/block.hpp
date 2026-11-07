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

      // Every field except the nonce, which is the only one that changes while mining
      std::string serializePrefix() const {
         return prevHash + "|" + std::to_string(timestamp) + "|"
                + std::to_string(version) + "|" + txRoot + "|"
                + std::to_string(difficulty) + "|";
      }

      std::string serialize() const {
         return serializePrefix() + std::to_string(nonce);
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
      hashes.reserve((txIds.size() + 1) / 2);
      std::string node;   // left bytes || right bytes, the same buffer for every pair
      for(int i = 0; i < txIds.size(); i += 2) {
         node.clear();
         appendBytes(node, txIds[i]);
         appendBytes(node, (i + 1 < txIds.size()) ? txIds[i + 1] : txIds[i]);
         hashes.push_back(hash::hashFunction(node));
      }

      return computeTxRoot(hashes);
   }

   static std::string hexToBytes(const std::string& hexStr) {
      std::string byteStr;
      appendBytes(byteStr, hexStr);
      return byteStr;
   }

   // Decodes hex to bytes at the end of `out`, which grows once instead of once per byte
   static void appendBytes(std::string& out, const std::string& hexStr) {
      if(hexStr.size() % 2 == 1) throw std::invalid_argument("hex input size can't be odd when decoding to bytes");
      const std::size_t start = out.size();
      out.resize(start + hexStr.size() / 2);
      for(int i = 0; i < hexStr.size(); i += 2) {
         char a = hexCharToByte(hexStr[i]);
         char b = hexCharToByte(hexStr[i+1]);
         out[start + i / 2] = a * 16 + b;
      }
   }

   static char hexCharToByte(char c) {
      if(c >= '0' && c <= '9') return c - '0';
      if(c >= 'a' && c <= 'f') return c - 'a' + 10;
      throw std::invalid_argument("char isn't a hex value");
   }

   static std::string computeTxRoot(const std::vector<Transaction>& transactions) {
      std::vector<std::string> txIds;
      txIds.reserve(transactions.size());
      for(const auto& tx : transactions) txIds.push_back(tx.getId());
      return computeTxRoot(txIds);
   }

   bool hasValidPow() const {
      return meetsDifficulty(hash(), header.difficulty);
   }

   // The hash starts with `difficulty` hex zeros
   static bool meetsDifficulty(const std::string& blockHash, std::uint32_t difficulty) {
      if(difficulty > blockHash.size()) return false;
      for(std::uint32_t i = 0; i < difficulty; ++i) {
         if(blockHash[i] != '0') return false;
      }
      return true;
   }

   void setNonce(std::uint64_t nonce) { header.nonce = nonce; }

   const Header& getHeader() const { return header; }
   const std::vector<Transaction>& getTransactions() const { return transactions; }

private:
   Header header;
   std::vector<Transaction> transactions;
};