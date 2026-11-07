#pragma once

#include <cstdint>
#include <limits>
#include <map>

#include "transaction.hpp"

class State {
private:
   static constexpr std::uint64_t MAX_TOTAL = std::numeric_limits<std::uint64_t>::max();

   std::map<Input, Output> unspent;

   void addOutputs(const Transaction& tx) {
      const auto& outputs = tx.getOutputs();
      for(unsigned int i = 0; i < outputs.size(); ++i) {
         unspent.emplace(Input{tx.getId(), i}, outputs[i]);
      }
   }

   void spend(const Transaction& tx) {
      for(const Input& in : tx.getInputs()) unspent.erase(in);
      addOutputs(tx);
   }

public:
   explicit State(const Transaction& funding) { addOutputs(funding); }

   // nullptr if `tx` can be applied, otherwise the reason why not. The reasons are fixed texts and nothing is
   // allocated, because candidate selection checks many transactions and most of them fail
   const char* findProblem(const Transaction& tx) const {
      const std::vector<Input>& inputs = tx.getInputs();
      const std::vector<Output>& outputs = tx.getOutputs();
      if(inputs.empty()) return "missing inputs";
      if(outputs.empty()) return "missing outputs";

      std::uint64_t inputTotal = 0;
      std::uint64_t outputTotal = 0;
      const std::string* owner = nullptr;

      for(std::size_t i = 0; i < inputs.size(); ++i) {
         // A transaction has few inputs, so comparing with the earlier ones is cheaper than building a set
         for(std::size_t j = 0; j < i; ++j) {
            if(inputs[j] == inputs[i]) return "input used twice";
         }
         auto it = unspent.find(inputs[i]);
         if(it == unspent.end()) return "unknown or spent output";

         const Output& spent = it->second; // Access old output amount (for spending)
         if(!owner) owner = &spent.ownerPK;
         else if(spent.ownerPK != *owner) return "inputs have different owners";
         if(spent.amount > MAX_TOTAL - inputTotal) return "input total overflows";
         inputTotal += spent.amount;
      }
      bool paysSomeoneElse = false;
      for(const Output& out : outputs) {
         if(out.amount == 0) return "zero amount output";
         if(out.ownerPK != *owner) paysSomeoneElse = true;
         if(out.amount > MAX_TOTAL - outputTotal) return "output total overflows";
         outputTotal += out.amount;
      }
      if(!paysSomeoneElse) return "owner pays only themselves";

      if(inputTotal != outputTotal) return "input and output totals don't match";

      return nullptr;
   }

   bool canApply(const Transaction& tx, std::string& reason) const {
      const char* problem = findProblem(tx);
      if(problem) reason = problem;
      return !problem;
   }

   bool apply(const Transaction& tx, std::string& reason) {
      if(!canApply(tx, reason)) {
         reason += " " + tx.getId();
         return false;
      }

      spend(tx);
      return true;
   }

   // apply() for callers that only need to know whether it worked
   bool tryApply(const Transaction& tx) {
      if(findProblem(tx)) return false;
      spend(tx);
      return true;
   }

   std::vector<std::pair<Input, Output>> getUnspentOutputs(const std::string& ownerPK) const {
      std::vector<std::pair<Input, Output>> unspentOutputs;
      for(const auto& entry : unspent) {
         if(entry.second.ownerPK == ownerPK) {
            unspentOutputs.push_back(entry);
         }
      }
      return unspentOutputs;
   }

   std::uint64_t total() const {
      std::uint64_t sum = 0;
      for(const auto& [ref, out] : unspent) sum += out.amount;
      return sum;
   }

   std::uint64_t balanceOf(const std::string& ownerPK) const {
      std::uint64_t sum = 0;
      for(const auto& [ref, out] : unspent) {
         if(out.ownerPK == ownerPK) sum += out.amount;
      }
      return sum;
   }

   std::size_t size() const { return unspent.size(); }
};