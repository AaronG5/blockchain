#pragma once

#include <string>
#include <cstdint>

#include "hash.hpp"

class Transaction {
private: 
   std::string sender; // Change to vector for UTXO
   std::string receiver; // Change to vector for UTXO
   std::uint64_t amount; // Add as tuple for sender and receiver vectors for UTXO
   std::uint64_t sequence; // Add 
   std::string id;

public:
   Transaction(std::string sender, std::string receiver, std::uint64_t amount, std::uint64_t sequence)
      : sender(std::move(sender)), receiver(std::move(receiver)), amount(amount), sequence(sequence), 
        id(computeId()) {}

   Transaction(std::string sender, std::string receiver, std::uint64_t amount, std::uint64_t sequence, std::string id)
      : sender(std::move(sender)), receiver(std::move(receiver)), amount(amount), sequence(sequence), 
        id(std::move(id)) {}


   std::string serialize() const {
      return sender + "|" + receiver + "|" + std::to_string(amount) + "|" + std::to_string(sequence);
   }

   std::string computeId() const {
      return hash::hashFunction(serialize());
   }

   bool hasValidId() const {
      return id == computeId();
   }

   const std::string& getSender() const { return sender; }
   const std::string& getReceiver() const { return receiver; }
   std::uint64_t getAmount() const { return amount; }
   std::uint64_t getSequence() const { return sequence; }
   const std::string& getId() const { return id; }
};