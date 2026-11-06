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
const std::string FINAL_BALANCES_PATH = "final_balances.csv";

struct Config {
   std::uint64_t seed = 123;
   std::size_t users = 1000;
   std::size_t transactions = 10000;
   std::size_t badPerKind = 5;
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

void printBlock(std::size_t height, const Block& block, std::uint64_t attempts, double timeSec) {
   const Block::Header& header = block.getHeader();

   std::cout << "Block " << height << "\n"
             << "   transactions: " << block.getTransactions().size() << "\n"
             << "   prev_hash:    " << header.prevHash << "\n"
             << "   tx_root:      " << header.txRoot << "\n"
             << "   hash:         " << block.hash() << "\n"
             << "   difficulty:   " << header.difficulty << "\n"
             << "   nonce:        " << header.nonce << "\n"
             << "   attempts:     " << attempts << "\n"
             << "   mining time:  " << std::fixed << std::setprecision(6) << timeSec << " s\n";
}

void printCandidates(const std::vector<Candidate>& candidates, const RaceResult& race) {
   std::cout << "   rounds:       " << race.rounds << "\n"
             << "   candidates:\n";
   for(std::size_t i = 0; i < candidates.size(); ++i) {
      const Candidate& candidate = candidates[i];
      std::cout << "      #" << i + 1 << ": "
                << std::setw(3) << candidate.block.getTransactions().size() << " txs, "
                << std::setw(6) << candidate.attempts << " attempts, "
                << std::fixed << std::setprecision(6) << candidate.timeSec << " s"
                << (static_cast<int>(i) == race.winner ? "  <- winner" : "") << "\n";
   }
}

bool mineGenesis(Blockchain& chain, std::string& reason) {
   Block genesis = chain.createBlock({});
   const MiningResult result = mine(genesis);
   if(!chain.addBlock(genesis, reason)) return false;

   printBlock(0, genesis, result.attempts, result.timeSec);
   std::cout << "\n";
   return true;
}

int run(const Config& config) {
   const auto start = std::chrono::steady_clock::now();

   Random rng(config.seed);
   const std::vector<User> users = generateUsers(config.users, rng);
   const Transaction funding = generateFunding(users, rng);
   const State initial(funding);
   saveBalances(INITIAL_BALANCES_PATH, users, initial);

   std::vector<Transaction> generated = generateTransactions(users, initial, config.transactions, rng);
   addBadTransactions(generated, config.badPerKind, rng);

   std::unordered_set<std::string> knownKeys;
   for(const User& user : users) knownKeys.insert(user.getPublicKey());
   Blockchain chain(initial, std::move(knownKeys));

   std::vector<Rejected> rejected;
   std::vector<Transaction> pool = filterPool(generated, chain, rejected);

   std::string reason;
   bool chainAccepted = mineGenesis(chain, reason);
   bool limitReached = false;
   std::uint64_t totalAttempts = 0;

   while(chainAccepted) {
      std::vector<Candidate> candidates = buildCandidates(pool, chain, rng, CANDIDATES_PER_BLOCK);
      if(candidates.empty()) break;

      const RaceResult race = mineCandidates(candidates, FIRST_ROUND_BUDGET, MAX_ATTEMPTS_PER_BLOCK);
      totalAttempts += race.totalAttempts;
      if(race.winner < 0) {
         limitReached = true;
         break;
      }

      const Candidate& winner = candidates[race.winner];
      chainAccepted = chain.addBlock(winner.block, reason);
      if(!chainAccepted) break;

      removeFromPool(pool, winner.block.getTransactions());

      printBlock(chain.size() - 1, winner.block, winner.attempts, winner.timeSec);
      printCandidates(candidates, race);
      std::cout << "   left in pool: " << pool.size() << "\n\n";
   }

   if(!chainAccepted) {
      std::cout << "Mined block was not accepted: " << reason << "\n\n";
   }
   if(limitReached) {
      std::cout << "Mining stopped: no candidate found a nonce within " << MAX_ATTEMPTS_PER_BLOCK << " attempts\n\n";
   }

   saveBalances(FINAL_BALANCES_PATH, users, chain.getConfirmed());

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
             << "   candidates per block:  " << CANDIDATES_PER_BLOCK << "\n"
             << "   first round budget:    " << FIRST_ROUND_BUDGET << " attempts per candidate, doubled each round\n"
             << "   attempts (candidates): " << totalAttempts << "\n"
             << "   total time:            " << std::fixed << std::setprecision(3) << totalSeconds << " s\n"
             << "   initial balances:      " << INITIAL_BALANCES_PATH << "\n"
             << "   final balances:        " << FINAL_BALANCES_PATH << "\n";

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

   return chainAccepted && !limitReached && fundsMatch && countsMatch ? 0 : 1;
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
