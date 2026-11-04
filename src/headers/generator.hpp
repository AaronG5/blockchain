#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hash.hpp"
#include "state.hpp"
#include "transaction.hpp"
#include "user.hpp"

class Random {
private:
   std::mt19937_64 engine;

public:
   explicit Random(std::uint64_t seed) : engine(seed) {}

   std::uint64_t next() { return engine(); }

   // Inclusive on both ends
   std::uint64_t between(std::uint64_t min, std::uint64_t max) {
      return min + engine() % (max - min + 1);
   }

   template <typename T>
   void shuffle(std::vector<T>& items) {
      for(std::size_t i = items.size(); i > 1; --i) {
         std::swap(items[i - 1], items[between(0, i - 1)]);
      }
   }
};

constexpr std::uint64_t MIN_INITIAL_BALANCE = 100;
constexpr std::uint64_t MAX_INITIAL_BALANCE = 1000000;

inline std::vector<User> generateUsers(std::size_t count, Random& rng) {
   std::vector<User> users;
   users.reserve(count);

   std::unordered_set<std::string> usedKeys;
   for(std::size_t i = 0; i < count; ++i) {
      const std::string username = "user" + std::to_string(i + 1);

      std::string publicKey;
      do {
         publicKey = hash::hashFunction(username + "|" + std::to_string(rng.next()));
      } while(!usedKeys.insert(publicKey).second);

      users.emplace_back(username, publicKey);
   }
   return users;
}

// Initial funds as initial UTXOs: a transaction with no inputs and one output per user, in the same
// order as `users`. It never goes into a block, only into State::fund.
inline Transaction generateFunding(const std::vector<User>& users, Random& rng) {
   std::vector<Output> outputs;
   outputs.reserve(users.size());
   for(const User& user : users) {
      const auto amount = rng.between(MIN_INITIAL_BALANCE, MAX_INITIAL_BALANCE);
      outputs.push_back({user.getPublicKey(), static_cast<unsigned int>(amount)});
   }
   return Transaction({}, std::move(outputs), 0);
}

inline void saveInitialBalances(const std::string& path, const std::vector<User>& users, const Transaction& funding) {
   std::ofstream file(path);
   if(!file) {
      throw std::runtime_error("Could not open " + path);
   }

   file << "username,public_key,amount\n";
   const std::vector<Output>& outputs = funding.getOutputs();
   for(std::size_t i = 0; i < users.size(); ++i) {
      file << users[i].getUserName() << "," << outputs[i].ownerPK << "," << outputs[i].amount << "\n";
   }
}

// Valid when applied in the returned order. `state` is a private copy that follows the payments, so
// later transactions can spend the outputs of earlier ones. Sequences are 1, 2, 3, ...
inline std::vector<Transaction> generateTransactions(const std::vector<User>& users, State state, std::size_t count, Random& rng) {
   if(users.size() < 2) {
      throw std::invalid_argument("Need at least 2 users to generate transactions");
   }

   std::vector<Transaction> transactions;
   transactions.reserve(count);

   while(transactions.size() < count) {
      const std::size_t senderIndex = rng.between(0, users.size() - 1);
      std::size_t receiverIndex = rng.between(0, users.size() - 2);
      if(receiverIndex >= senderIndex) ++receiverIndex;   // anyone except the sender

      const std::string sender = users[senderIndex].getPublicKey();
      const std::string receiver = users[receiverIndex].getPublicKey();

      auto available = state.getUnspentOutputs(sender);
      if(available.empty()) continue;   // spent everything and received nothing yet
      rng.shuffle(available);

      std::uint64_t balance = 0;
      for(const auto& entry : available) balance += entry.second.amount;
      const std::uint64_t amount = rng.between(1, balance);

      // Outputs are spent whole, so take them until they cover the amount and return the rest as change
      std::vector<Input> inputs;
      std::uint64_t collected = 0;
      for(const auto& entry : available) {
         inputs.push_back(entry.first);
         collected += entry.second.amount;
         if(collected >= amount) break;
      }

      std::vector<Output> outputs{{receiver, static_cast<unsigned int>(amount)}};
      if(collected > amount) {
         outputs.push_back({sender, static_cast<unsigned int>(collected - amount)});
      }

      Transaction tx(std::move(inputs), std::move(outputs), static_cast<unsigned int>(transactions.size() + 1));

      std::string reason;
      if(!state.apply(tx, reason)) {
         throw std::logic_error("Generated an invalid transaction: " + reason);
      }
      transactions.push_back(std::move(tx));
   }
   return transactions;
}

// Appends `perKind` transactions of each kind, each built from a valid one in `transactions`:
//    - double spend: same inputs as the valid one, so only one of the two can ever confirm
//    - tampered id: the stored id does not match the content
//    - zero amount: has an output worth 0
//    - unknown receiver: pays a public key that belongs to no user
// Double spends only copy transactions whose outputs nothing spends. If the copy confirms first, exactly
// one transaction is stuck; copying any other would also strand everything that spends its outputs.
inline void addBadTransactions(std::vector<Transaction>& transactions, std::size_t perKind, Random& rng) {
   if(transactions.empty()) {
      throw std::invalid_argument("Need valid transactions to build bad ones from");
   }

   std::unordered_set<std::string> spentFrom;
   for(const Transaction& tx : transactions) {
      for(const Input& in : tx.getInputs()) spentFrom.insert(in.transactionId);
   }

   // Never empty: nothing comes after the last transaction, so nothing spends its outputs
   std::vector<std::size_t> unspentIndexes;
   for(std::size_t i = 0; i < transactions.size(); ++i) {
      if(spentFrom.count(transactions[i].getId()) == 0) unspentIndexes.push_back(i);
   }

   // Enough room for all of them, so `base` below stays valid while appending
   transactions.reserve(transactions.size() + perKind * 4);

   auto randomBase = [&]() -> const Transaction& {
      return transactions[unspentIndexes[rng.between(0, unspentIndexes.size() - 1)]];
   };
   auto nextSequence = [&]() { return static_cast<unsigned int>(transactions.size() + 1); };

   for(std::size_t i = 0; i < perKind; ++i) {
      {
         const Transaction& base = randomBase();
         std::vector<Output> outputs{{base.getOutputs()[0].ownerPK, static_cast<unsigned int>(base.outputTotal())}};
         transactions.emplace_back(base.getInputs(), std::move(outputs), nextSequence());
      }
      {
         const Transaction& base = randomBase();
         const unsigned int sequence = nextSequence();
         transactions.emplace_back(base.getInputs(), base.getOutputs(), sequence, hash::hashFunction("tampered " + std::to_string(sequence)));
      }
      {
         const Transaction& base = randomBase();
         std::vector<Output> outputs = base.getOutputs();
         outputs.push_back({outputs[0].ownerPK, 0});
         transactions.emplace_back(base.getInputs(), std::move(outputs), nextSequence());
      }
      {
         const Transaction& base = randomBase();
         const unsigned int sequence = nextSequence();
         std::vector<Output> outputs = base.getOutputs();
         outputs[0].ownerPK = hash::hashFunction("unknown receiver " + std::to_string(sequence));
         transactions.emplace_back(base.getInputs(), std::move(outputs), sequence);
      }
   }
}
