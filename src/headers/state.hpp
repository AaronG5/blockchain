#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <set>

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

public:
   explicit State(const Transaction& funding) { addOutputs(funding); }

   bool canApply(const Transaction& tx, std::string& reason) const {
      std::uint64_t inputTotal = 0;
      std::uint64_t outputTotal = 0;
      std::set<Input> seen;
      std::string owner;

      if(tx.getInputs().empty()) { reason = "missing inputs"; return false; }
      if(tx.getOutputs().empty()) { reason = "missing outputs"; return false; }

      for(const Input& in : tx.getInputs()) {
         if(!seen.insert(in).second) { reason = "input used twice"; return false; }
         auto it = unspent.find(in);
         if(it == unspent.end()) { reason = "unknown or spent output"; return false; }

         const Output& spent = it->second; // Access old output amount (for spending)
         if(owner.empty()) owner = spent.ownerPK;
         else if(spent.ownerPK != owner) { reason = "inputs have different owners"; return false; }
         if(spent.amount > MAX_TOTAL - inputTotal) { reason = "input total overflows"; return false; }
         inputTotal += spent.amount;
      }
      bool paysSomeoneElse = false;
      for(const Output& out : tx.getOutputs()) {
         if(out.amount == 0) { reason = "zero amount output"; return false; }
         if(out.ownerPK != owner) paysSomeoneElse = true;
         if(out.amount > MAX_TOTAL - outputTotal) { reason = "output total overflows"; return false; }
         outputTotal += out.amount;
      }
      if(!paysSomeoneElse) { reason = "owner pays only themselves"; return false; }

      if(inputTotal != outputTotal) { reason = "input and output totals don't match"; return false; }

      return true;
   }

   bool apply(const Transaction& tx, std::string& reason) {
      if(!canApply(tx, reason)) {
         reason += " " + tx.getId();
         return false;
      }

      for(const Input& in : tx.getInputs()) unspent.erase(in);
      addOutputs(tx);
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