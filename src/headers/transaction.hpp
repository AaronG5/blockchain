#pragma once

#include <string>
#include <cstdint>

#include "hash.hpp"

struct Input {
   std::string transactionId;
   unsigned int index;

   bool operator< (const Input& inp) const {
      return std::tie(transactionId, index) < std::tie(inp.transactionId, inp.index);
   }
   bool operator== (const Input& inp) const {
      return transactionId == inp.transactionId && index == inp.index;
   }
};

struct Output {
   std::string ownerPK;
   unsigned int amount;
};

class Transaction {
private: 
   std::vector<Input> inputs;
   std::vector<Output> outputs;
   unsigned int sequence;
   std::string id;

public:
   Transaction(std::vector<Input> inputs, std::vector<Output> outputs, unsigned int sequence)
      : inputs(std::move(inputs)), outputs(std::move(outputs)), sequence(sequence), id(computeId()) {}

   Transaction(std::vector<Input> inputs, std::vector<Output> outputs, unsigned int sequence, std::string id)
      : inputs(std::move(inputs)), outputs(std::move(outputs)), sequence(sequence), id(std::move(id)) {}

   // transactionId:index;transactionId:index|ownerPK:amount;ownerPK:amount|sequence
   std::string serialize() const {
      std::string acc;
      for(int i = 0; i < inputs.size(); ++i) {
         if(i) acc += ";";
         acc += inputs[i].transactionId + ":" + std::to_string(inputs[i].index);
      }

      acc += "|";

      for(int i = 0; i < outputs.size(); ++i) {
         if(i) acc += ";";
         acc += outputs[i].ownerPK + ":" + std::to_string(outputs[i].amount);
      }

      return acc + "|" + std::to_string(sequence);
   }

   std::string computeId() const {
      return hash::hashFunction(serialize());
   }

   std::uint64_t outputTotal() const {
      std::uint64_t sum = 0;
      for(auto out : outputs) sum += out.amount;
      return sum;
   }

   bool hasValidId() const { return id == computeId(); }

   const std::vector<Input>& getInputs() const { return inputs; }
   const std::vector<Output>& getOutputs() const { return outputs; }
   unsigned int getSequence() const { return sequence; }
   const std::string& getId() const { return id; }
};