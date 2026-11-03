#pragma once

#include <map>

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
};