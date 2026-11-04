#pragma once

#include <map>
#include <set>

#include "transaction.hpp"

class State {
private:
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
         inputTotal += spent.amount;
      }
      bool paysSomeoneElse = false;
      for(const Output& out : tx.getOutputs()) {
         if(out.amount == 0) { reason = "zero amount output"; return false; }
         if(out.ownerPK != owner) paysSomeoneElse = true;
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
};