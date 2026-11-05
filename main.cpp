#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/headers/blockchain.hpp"
#include "src/headers/generator.hpp"
#include "src/headers/helpers.hpp"

const std::string INITIAL_BALANCES_PATH = "initial_balances.csv";

struct Config {
   std::uint64_t seed = 123;
   std::size_t users = 1000;
   std::size_t transactions = 10000;
   std::size_t badPerKind = 5;
};

struct Rejected {
   Transaction tx;
   std::string reason;
};

// Usage: blockchain [seed] [users] [transactions] [bad transactions per kind]
Config readConfig(int argc, char* argv[]) {
   if(argc > 5) {
      throw std::invalid_argument("Too many arguments");
   }

   Config config;
   if(argc > 1) config.seed = std::stoull(argv[1]);
   if(argc > 2) config.users = std::stoull(argv[2]);
   if(argc > 3) config.transactions = std::stoull(argv[3]);
   if(argc > 4) config.badPerKind = std::stoull(argv[4]);
   return config;
}

std::string findPermanentProblem(const Transaction& tx, const std::unordered_set<std::string>& knownKeys) {}

std::vector<Transaction> filterPool(const std::vector<Transaction>& generated, const std::vector<User>& users, std::vector<Rejected>& rejected) {}

void removeFromPool(std::vector<Transaction>& pool, const std::vector<Transaction>& included) {}

void printBlock(std::size_t height, const Block& block, const MiningResult& result) {}

bool mineAndAdd(Blockchain& chain, const std::vector<Transaction>& transactions, std::string& reason) {}

int run(const Config& config) {
   const auto start = std::chrono::steady_clock::now();

   Random rng(config.seed);
   const std::vector<User> users = generateUsers(config.users, rng);
   const Transaction funding = generateFunding(users, rng);
   const State initial(funding);
   saveInitialBalances(INITIAL_BALANCES_PATH, users, funding);

   std::vector<Transaction> generated = generateTransactions(users, initial, config.transactions, rng);
   addBadTransactions(generated, config.badPerKind, rng);

   std::vector<Rejected> rejected;
   std::vector<Transaction> pool = filterPool(generated, users, rejected);

   Blockchain chain(initial);
   std::string reason;

   // Genesis block
   bool chainAccepted = mineAndAdd(chain, {}, reason);

   while(chainAccepted) {
      const std::vector<Transaction> selected = selectTransactions(pool, chain.getConfirmed(), rng);
      if(selected.empty()) break;

      chainAccepted = mineAndAdd(chain, selected, reason);
      if(chainAccepted) removeFromPool(pool, selected);
   }

   if(!chainAccepted) {
      std::cout << "Mined block was not accepted: " << reason << "\n\n";
   }

   std::size_t included = 0;
   for(const Block& block : chain.getChain()) included += block.getTransactions().size();

   const std::uint64_t initialFunds = chain.getInitial().total();
   const std::uint64_t finalFunds = chain.getConfirmed().total();
   const bool fundsMatch = initialFunds == finalFunds;
   const bool countsMatch = included + rejected.size() + pool.size() == generated.size();

   const auto end = std::chrono::steady_clock::now();
   const double totalSeconds = std::chrono::duration<double>(end - start).count();

   std::cout << "Summary\n"
             << "   seed:                  " << config.seed << "\n"
             << "   users:                 " << users.size() << "\n"
             << "   transactions:          " << generated.size() << "\n"
             << "   blocks (with genesis): " << chain.size() << "\n"
             << "   included:              " << included << "\n"
             << "   rejected:              " << rejected.size() << "\n"
             << "   remaining in pool:     " << pool.size() << "\n"
             << "   initial funds:         " << initialFunds << "\n"
             << "   final funds:           " << finalFunds << "\n"
             << "   funds match:           " << (fundsMatch ? "yes" : "NO") << "\n"
             << "   counts add up:         " << (countsMatch ? "yes" : "NO") << "\n"
             << "   total time:            " << std::fixed << std::setprecision(3) << totalSeconds << " s\n"
             << "   initial balances:      " << INITIAL_BALANCES_PATH << "\n";

   if(!rejected.empty()) {
      std::cout << "\nRejected transactions\n";
      for(const Rejected& entry : rejected) {
         std::cout << "   " << entry.tx.getId() << ": " << entry.reason << "\n";
      }
   }

   if(!pool.empty()) {
      std::cout << "\nRemaining transactions\n";
      for(const Transaction& tx : pool) {
         std::string why = "can still be applied";
         chain.getConfirmed().canApply(tx, why);
         std::cout << "   " << tx.getId() << ": " << why << "\n";
      }
   }

   return chainAccepted && fundsMatch && countsMatch ? 0 : 1;
}

int main(int argc, char* argv[]) {
   try {
      return run(readConfig(argc, argv));
   }
   catch(const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n"
                << "Usage: blockchain [seed] [users] [transactions] [bad transactions per kind]\n";
      return 1;
   }
}
