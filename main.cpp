#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/headers/blockchain.hpp"
#include "src/headers/generator.hpp"
#include "src/headers/helpers.hpp"

const std::string INITIAL_BALANCES_PATH = "initial_balances.csv";
const std::string FINAL_BALANCES_PATH = "final_balances.csv";
const std::string BENCHMARK_PATH = "benchmark.csv";
constexpr int BENCHMARK_RUNS = 5;
constexpr std::uint64_t FIXED_WORK_ATTEMPTS = 500000;

struct Config {
   std::uint64_t seed = 123;
   std::size_t users = 1000;
   std::size_t transactions = 10000;
   std::size_t badPerKind = 5;
   bool parallel = false;
   bool benchmark = false;
};

struct RunStats {
   int exitCode;
   std::size_t blocks;
   std::uint64_t attempts;
   double miningSec;
   double totalSec;
};

// Usage: blockchain [seed] [users] [transactions] [bad transactions per kind] [--parallel] [--benchmark]
Config readConfig(int argc, char* argv[]) {
   Config config;
   std::vector<std::string> numbers;
   for(int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if(arg == "--parallel") config.parallel = true;
      else if(arg == "--benchmark") config.benchmark = true;
      else numbers.push_back(arg);
   }

   if(numbers.size() > 4) {
      throw std::invalid_argument("Too many arguments");
   }
   if(numbers.size() > 0) config.seed = std::stoull(numbers[0]);
   if(numbers.size() > 1) config.users = std::stoull(numbers[1]);
   if(numbers.size() > 2) config.transactions = std::stoull(numbers[2]);
   if(numbers.size() > 3) config.badPerKind = std::stoull(numbers[3]);
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
   if(race.rounds > 0) std::cout << "   rounds:       " << race.rounds << "\n";
   std::cout << "   candidates:\n";
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
   std::cout << std::endl;
   return true;
}

RunStats run(const Config& config) {
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
   double miningSeconds = 0;

   while(chainAccepted) {
      std::vector<Candidate> candidates = buildCandidates(pool, chain, rng, CANDIDATES_PER_BLOCK);
      if(candidates.empty()) break;

      const RaceResult race = config.parallel
         ? mineCandidatesParallel(candidates, MAX_ATTEMPTS_PER_BLOCK)
         : mineCandidates(candidates, FIRST_ROUND_BUDGET, MAX_ATTEMPTS_PER_BLOCK);
      totalAttempts += race.totalAttempts;
      miningSeconds += race.timeSec;
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
      std::cout << "   left in pool: " << pool.size() << "\n" << std::endl;
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
             << "   funds match:           " << (fundsMatch ? "YES" : "NO") << "\n"
             << "   counts add up:         " << (countsMatch ? "YES" : "NO") << "\n"
             << "   candidates per block:  " << CANDIDATES_PER_BLOCK << "\n"
             << "   first round budget:    " << (config.parallel ? std::string("none, threads mine without rounds")
                                                  : std::to_string(FIRST_ROUND_BUDGET) + " attempts per candidate, doubled each round") << "\n"
             << "   mining mode:           " << (config.parallel ? "parallel (one thread per candidate)" : "sequential") << "\n"
             << "   attempts (candidates): " << totalAttempts << "\n"
             << "   candidate mining time: " << std::fixed << std::setprecision(3) << miningSeconds << " s\n"
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

   const int exitCode = chainAccepted && !limitReached && fundsMatch && countsMatch ? 0 : 1;
   return {exitCode, static_cast<std::size_t>(chain.size()), totalAttempts, miningSeconds, totalSeconds};
}

// Mean and sample standard deviation
std::pair<double, double> meanAndSpread(const std::vector<double>& values) {
   double mean = 0;
   for(double value : values) mean += value;
   mean /= values.size();

   double squares = 0;
   for(double value : values) squares += (value - mean) * (value - mean);
   return {mean, values.size() > 1 ? std::sqrt(squares / (values.size() - 1)) : 0.0};
}

// The same work in every run: CANDIDATES_PER_BLOCK fixed headers at difficulty 20 (a chance of 16^-20 per attempt,
// so in practice never met), mined for exactly FIXED_WORK_ATTEMPTS nonces. Unlike a normal run, its time doesn't
// depend on how lucky the nonces are
RunStats fixedWork(bool parallel) {
   std::vector<Candidate> candidates;
   for(std::size_t i = 0; i < CANDIDATES_PER_BLOCK; ++i) {
      candidates.push_back({Block(std::string(64, '0'), static_cast<std::int64_t>(1700000000 + i), 2, {}, 20)});
   }

   const RaceResult race = parallel ? mineCandidatesParallel(candidates, FIXED_WORK_ATTEMPTS)
                                    : mineCandidates(candidates, FIRST_ROUND_BUDGET, FIXED_WORK_ATTEMPTS);
   const int exitCode = race.winner < 0 && race.totalAttempts == FIXED_WORK_ATTEMPTS ? 0 : 1;
   return {exitCode, 0, race.totalAttempts, race.timeSec, race.timeSec};
}

// Runs the same config BENCHMARK_RUNS times sequentially, then in parallel, then BENCHMARK_RUNS fixed-work runs in
// both modes, and writes every run plus the mean and standard deviation to BENCHMARK_PATH. The normal block
// output is hidden during the runs
int benchmark(Config config) {
   std::ofstream file(BENCHMARK_PATH);
   if(!file) {
      throw std::runtime_error("Could not open " + BENCHMARK_PATH);
   }
   file << std::fixed << std::setprecision(6) << "mode,run,blocks,attempts,mining_time_s,total_time_s,attempts_per_s\n";

   struct Mode {
      std::string name;
      bool parallel;
      bool fixed;
   };
   const Mode modes[4] = {{"sequential", false, false}, {"parallel", true, false},
                          {"sequential_fixed", false, true}, {"parallel_fixed", true, true}};

   bool allOk = true;
   double meanMining[4] = {0, 0, 0, 0};

   for(int m = 0; m < 4; ++m) {
      const Mode& mode = modes[m];
      config.parallel = mode.parallel;
      std::vector<double> columns[5];   // blocks, attempts, mining time, total time, attempts per second

      for(int i = 1; i <= BENCHMARK_RUNS; ++i) {
         std::cout.setstate(std::ios::badbit);   // while set, everything written to std::cout is dropped
         const RunStats stats = mode.fixed ? fixedWork(mode.parallel) : run(config);
         std::cout.clear();

         allOk = allOk && stats.exitCode == 0;
         const double rate = stats.attempts / stats.miningSec;
         const double values[5] = {double(stats.blocks), double(stats.attempts), stats.miningSec, stats.totalSec, rate};
         file << mode.name << "," << i << "," << stats.blocks << "," << stats.attempts << ","
              << stats.miningSec << "," << stats.totalSec << "," << rate << "\n";
         for(int c = 0; c < 5; ++c) columns[c].push_back(values[c]);

         std::cout << std::fixed << std::setprecision(3) << mode.name << " run " << i << ": mining " << stats.miningSec
                   << " s, total " << stats.totalSec << " s, " << rate / 1e6 << " M attempts/s" << std::endl;
      }

      file << mode.name << ",mean";
      for(const auto& column : columns) file << "," << meanAndSpread(column).first;
      file << "\n" << mode.name << ",stddev";
      for(const auto& column : columns) file << "," << meanAndSpread(column).second;
      file << "\n";

      const auto [mean, spread] = meanAndSpread(columns[2]);
      const auto [rateMean, rateSpread] = meanAndSpread(columns[4]);
      meanMining[m] = mean;
      std::cout << mode.name << " mining time: " << mean << " s on average, std dev " << spread << " s, "
                << rateMean / 1e6 << " +- " << rateSpread / 1e6 << " M attempts/s\n\n";
   }

   std::cout << "parallel speedup (mining): " << std::setprecision(2) << meanMining[0] / meanMining[1] << "x\n"
             << "parallel speedup (fixed work): " << meanMining[2] / meanMining[3] << "x\n"
             << "results: " << BENCHMARK_PATH << "\n";
   return allOk ? 0 : 1;
}

int main(int argc, char* argv[]) {
   try {
      const Config config = readConfig(argc, argv);
      return config.benchmark ? benchmark(config) : run(config).exitCode;
   }
   catch(const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n"
                << "Usage: blockchain [seed] [users] [transactions] [bad transactions per kind] [--parallel] [--benchmark]\n";
      return 1;
   }
}
