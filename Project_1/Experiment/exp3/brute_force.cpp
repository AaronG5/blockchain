// Preimage search over a small public candidate set (all 4-digit strings "0000" to "9999").
//
// The attack only gets the target hash, the candidate set and (for the salted case) the salt,
// never the chosen input. Every candidate is tried, so all matching candidates are found.
//
// 1. No salt:    H(input)
// 2. Public salt: H(input || salt), with a random 16-byte salt written as 32 hex characters.
//    Also compares reusing a precomputed table of unsalted hashes with brute-forcing
//    several targets that each have their own salt.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../hash.hpp"

namespace fs = std::filesystem;

// The target inputs and salts come from std::mt19937_64 seeded with SEED
const std::uint64_t SEED = 20261007;
const int SALT_BYTES = 16;
const int REUSE_TARGETS = 5;   // targets used to compare a precomputed table with per-salt brute force

struct AttackResult {
   std::uint64_t attempts = 0;
   std::uint64_t attemptsToFirst = 0;
   double seconds = 0.0;
   double secondsToFirst = 0.0;
   std::vector<std::string> matches;
};

static std::vector<std::string> buildCandidates() {
   std::vector<std::string> candidates;
   for (int i = 0; i <= 9999; ++i) {
      std::ostringstream ss;
      ss << std::setw(4) << std::setfill('0') << i;
      candidates.push_back(ss.str());
   }
   return candidates;
}

static std::string randomSaltHex(std::mt19937_64& rng) {
   std::ostringstream ss;
   for (int i = 0; i < SALT_BYTES; ++i) {
      ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rng() & 0xFF);
   }
   return ss.str();
}

// Tries every candidate and records all of them whose hash equals the target hash
static AttackResult attack(const std::string& targetHash, const std::vector<std::string>& candidates,
                           const std::string& saltHex = "") {
   AttackResult r;
   auto start = std::chrono::steady_clock::now();

   for (const auto& candidate : candidates) {
      ++r.attempts;
      if (hashFunction(candidate, saltHex) == targetHash) {
         if (r.matches.empty()) {
            r.attemptsToFirst = r.attempts;
            r.secondsToFirst = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
         }
         r.matches.push_back(candidate);
      }
   }

   r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
   return r;
}

static std::string joinMatches(const std::vector<std::string>& matches) {
   if (matches.empty()) return "none";
   std::string s;
   for (const auto& m : matches) s += (s.empty() ? "" : ", ") + ("\"" + m + "\"");
   return s;
}

static void writeAttack(std::ostream& out, const AttackResult& r, const std::string& target) {
   out << "Attempts (whole set):       " << r.attempts << "\n";
   out << "Attempts until first match: " << r.attemptsToFirst << "\n";
   out << "Time (whole set):           " << r.seconds * 1000 << " ms\n";
   out << "Time until first match:     " << r.secondsToFirst * 1000 << " ms\n";
   out << "Matching candidates (" << r.matches.size() << "):    " << joinMatches(r.matches) << "\n";
   bool found = std::find(r.matches.begin(), r.matches.end(), target) != r.matches.end();
   out << "Chosen input among matches: " << (found ? "yes" : "no") << "\n";
}

int main(int argc, char* argv[]) {
   fs::path baseDir = (argc > 1) ? fs::path(argv[1]) : fs::absolute(argv[0]).lexically_normal().parent_path();

   std::mt19937_64 rng(SEED);
   const std::vector<std::string> candidates = buildCandidates();

   // Untimed warm-up run to load image to cache
   hashFunction("warm-up");

   std::ofstream report(baseDir / "exp3_results.txt");
   report << std::fixed << std::setprecision(3);
   report << "Candidates: " << candidates.size() << " four-digit strings, \"0000\" to \"9999\"\n";
   report << "Target inputs and salts: std::mt19937_64, seed " << SEED << "\n";
   report << "Salt: " << SALT_BYTES << " random bytes, written as " << SALT_BYTES * 2
          << " lowercase hex characters and appended to the input as raw bytes\n\n";

   // 1. No salt
   const std::string target = candidates[rng() % candidates.size()];
   const std::string targetHash = hashFunction(target);
   std::cerr << "No salt...\n";
   AttackResult plain = attack(targetHash, candidates);

   report << "=== 1. No salt: H(input) ===\n\n";
   report << "Chosen input (hidden from the attack): \"" << target << "\"\n";
   report << "Target hash:                " << targetHash << "\n";
   writeAttack(report, plain, target);

   // 2. Public salt
   const std::string salt = randomSaltHex(rng);
   const std::string saltedHash = hashFunction(target, salt);
   std::cerr << "Public salt...\n";
   AttackResult salted = attack(saltedHash, candidates, salt);

   report << "\n=== 2. Public salt: H(input || salt) ===\n\n";
   report << "Chosen input (hidden from the attack): \"" << target << "\"\n";
   report << "Salt (known to the attack): " << salt << "\n";
   report << "Target hash:                " << saltedHash << "\n";
   writeAttack(report, salted, target);

   // Reusing precomputed results: one unsalted table against targets that each have their own salt
   std::cerr << "Precomputed table...\n";
   auto start = std::chrono::steady_clock::now();
   std::unordered_map<std::string, std::vector<std::string>> table;
   for (const auto& c : candidates) table[hashFunction(c)].push_back(c);
   double tableSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

   report << "\n=== Reusing precomputed results ===\n\n";
   report << "Unsalted table: " << candidates.size() << " hashes, computed once in " << tableSeconds * 1000 << " ms\n\n";

   std::uint64_t saltedAttempts = 0;
   double saltedSeconds = 0.0;
   for (int t = 0; t < REUSE_TARGETS; ++t) {
      const std::string input = candidates[rng() % candidates.size()];
      const std::string s = randomSaltHex(rng);
      const std::string h = hashFunction(input, s);

      // Unsalted target: answered by a table lookup, no new hashes
      auto plainHit = table.find(hashFunction(input));
      std::string plainMatches = (plainHit == table.end()) ? "none" : joinMatches(plainHit->second);

      // Salted target: the unsalted table does not help, so every candidate is hashed again with this salt
      std::size_t tableMatches = table.count(h) ? table.at(h).size() : 0;
      AttackResult r = attack(h, candidates, s);
      saltedAttempts += r.attempts;
      saltedSeconds += r.seconds;

      report << "Target " << t + 1 << ": input \"" << input << "\" (hidden from the attack), salt " << s << "\n";
      report << "   Without salt: table lookup finds " << plainMatches << ", 0 new hashes\n";
      report << "   With salt:    table lookup finds " << tableMatches << " matches, so brute force: "
             << r.attempts << " attempts, " << r.seconds * 1000 << " ms, finds " << joinMatches(r.matches) << "\n\n";
   }

   report << "Total for " << REUSE_TARGETS << " targets:\n";
   report << "   Without salt: " << candidates.size() << " hashes (the table), " << tableSeconds * 1000 << " ms\n";
   report << "   With salt:    " << saltedAttempts << " hashes, " << saltedSeconds * 1000 << " ms\n\n";

   std::cerr << "Done. Results: " << (baseDir / "exp3_results.txt").string() << "\n";
   return 0;
}
