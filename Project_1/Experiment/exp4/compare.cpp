// Compares the original hash (hash.hpp) with the improved one (hash_v2.hpp) by running the tests of
// experiments 1-3 on both, with the same inputs and seeds as before.
//
// 1. Correctness (exp1): hash length, determinism and empty input for the files in exp1/gen
// 2. Speed (exp1): average time of 10 runs for every constitution snippet in exp1/gen_konst
// 3. Collisions (exp2): random pairs, full sets and the structured input sets
// 4. Avalanche (exp2): pairs that differ in exactly one character
// 5. Brute force (exp3): all four-digit candidates, without and with a public salt
//
// Usage (from Project_1, because both hashes read image.png from the working directory):
//    ./Experiment/exp4/compare [baseDir] [pairs]

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../hash.hpp"      // must come before hash_v2.hpp, it compiles stb_image
#include "../../hash_v2.hpp"

namespace fs = std::filesystem;

struct HashImpl {
   std::string name;
   std::string (*function)(const std::string&, const std::string&);
};

static const std::vector<HashImpl> HASHES = {
   {"Original", hashFunction},
   {"Improved", hash_v2::hashFunction},
};

// Same alphabet, seeds and sizes as experiments 2 and 3
static const std::string ALPHABET = [] {
   std::string s;
   for (int c = 0x20; c <= 0x7E; ++c) s += static_cast<char>(c);
   return s;
}();

const std::uint64_t SEED = 20261007;
const std::vector<int> LENGTHS = {10, 100, 500, 1000};
const int DEFAULT_PAIRS = 100000;
const int HASH_HEX = 64;
const int HASH_BITS = HASH_HEX * 4;
const int BENCH_RUNS = 10;
const int SALT_BYTES = 16;

static std::size_t randomIndex(std::mt19937_64& rng, std::size_t n) {
   const std::uint64_t limit = std::numeric_limits<std::uint64_t>::max() - std::numeric_limits<std::uint64_t>::max() % n;
   std::uint64_t r;
   do {
      r = rng();
   } while (r >= limit);
   return r % n;
}

static std::string randomString(std::mt19937_64& rng, int length) {
   std::string s(length, ' ');
   for (char& c : s) c = ALPHABET[randomIndex(rng, ALPHABET.size())];
   return s;
}

static std::string readFile(const fs::path& p) {
   std::ifstream f(p, std::ios::binary);
   std::stringstream ss;
   ss << f.rdbuf();
   return ss.str();
}

static double secondsSince(std::chrono::steady_clock::time_point start) {
   return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

static std::vector<std::string> hashAll(const HashImpl& h, const std::vector<std::string>& inputs) {
   std::vector<std::string> hashes;
   hashes.reserve(inputs.size());
   for (const auto& in : inputs) hashes.push_back(h.function(in, ""));
   return hashes;
}

static void writeTitle(std::ostream& out, const std::string& title) {
   out << "\n=== " << title << " ===\n\n";
}

// ---------------------------------------------------------------------------------------------
// 1. Correctness
// ---------------------------------------------------------------------------------------------

// "OK" if the hash has 64 lowercase hex characters and hashing again gives the same hash
static std::string checkHash(const HashImpl& h, const std::string& input) {
   try {
      std::string first = h.function(input, "");
      if (first.size() != HASH_HEX || first.find_first_not_of("0123456789abcdef") != std::string::npos) return "BAD FORMAT";
      if (h.function(input, "") != first) return "NOT DETERMINISTIC";
      return "OK";
   } catch (const std::exception& e) {
      return std::string("REJECTED (") + e.what() + ")";
   }
}

static void runCorrectness(std::ostream& report, const fs::path& genDir) {
   writeTitle(report, "1. Correctness: files from exp1/gen");

   std::vector<fs::path> files;
   if (fs::is_directory(genDir)) {
      for (const auto& entry : fs::directory_iterator(genDir)) {
         if (entry.is_regular_file() && entry.path().extension() == ".txt") files.push_back(entry.path());
      }
   }
   if (files.empty()) {
      report << "No .txt files in " << genDir.string() << ", run Experiment/exp1/gen_files.py first.\n";
   } else {
      std::sort(files.begin(), files.end());
      report << std::left << std::setw(32) << "File" << std::right << std::setw(6) << "Bytes" << "   "
             << std::left << std::setw(28) << HASHES[0].name << HASHES[1].name << "\n";
      report << std::string(80, '-') << "\n";
      for (const auto& file : files) {
         std::string content = readFile(file);
         report << std::left << std::setw(32) << file.filename().string() << std::right << std::setw(6) << content.size()
                << "   " << std::left << std::setw(28) << checkHash(HASHES[0], content) << checkHash(HASHES[1], content)
                << std::right << "\n";
      }
      report << "\nOK: 64 lowercase hex characters, and the same hash when hashed again\n";
   }

   report << "\nExample hashes:\n";
   for (const std::string input : {"", "a", "b", "ab", "ba"}) {
      report << "   \"" << input << "\"\n";
      for (const auto& h : HASHES) {
         std::string result;
         try {
            result = h.function(input, "");
         } catch (const std::exception& e) {
            result = std::string("error: ") + e.what();
         }
         report << "      " << std::left << std::setw(10) << h.name << std::right << result << "\n";
      }
   }
}

// ---------------------------------------------------------------------------------------------
// 2. Speed
// ---------------------------------------------------------------------------------------------

static int fileNumber(const fs::path& p) {
   std::string stem = p.stem().string();
   std::size_t pos = stem.find('_');
   return (pos == std::string::npos) ? 0 : std::stoi(stem.substr(pos + 1));
}

static double averageSeconds(const HashImpl& h, const std::string& content, std::size_t& sink) {
   sink += h.function(content, "").size();   // untimed warm-up run
   double total = 0.0;
   for (int i = 0; i < BENCH_RUNS; ++i) {
      auto start = std::chrono::steady_clock::now();
      sink += h.function(content, "").size();
      total += secondsSince(start);
   }
   return total / BENCH_RUNS;
}

static void runSpeed(std::ostream& report, const fs::path& konstDir, const fs::path& outDir) {
   writeTitle(report, "2. Speed: constitution snippets from exp1/gen_konst");

   std::vector<fs::path> files;
   if (fs::is_directory(konstDir)) {
      for (const auto& entry : fs::directory_iterator(konstDir)) {
         if (entry.is_regular_file() && entry.path().extension() == ".txt") files.push_back(entry.path());
      }
   }
   if (files.empty()) {
      report << "No .txt files in " << konstDir.string() << ", run Experiment/exp1/gen_konst.py first.\n";
      return;
   }
   std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
      return fileNumber(a) < fileNumber(b);
   });

   std::ofstream csv(outDir / "bench.csv");
   csv << "filename,size_bytes,original_seconds,improved_seconds\n" << std::setprecision(12);

   report << std::left << std::setw(18) << "File" << std::right << std::setw(10) << "Bytes"
          << std::setw(16) << (HASHES[0].name + " ms") << std::setw(16) << (HASHES[1].name + " ms") << "\n";
   report << std::string(60, '-') << "\n";
   std::size_t sink = 0;
   for (const auto& file : files) {
      std::string content = readFile(file);
      double original = averageSeconds(HASHES[0], content, sink);
      double improved = averageSeconds(HASHES[1], content, sink);
      csv << file.filename().string() << ',' << content.size() << ',' << original << ',' << improved << '\n';
      report << std::left << std::setw(18) << file.filename().string() << std::right << std::setw(10) << content.size()
             << std::setprecision(4) << std::setw(16) << original * 1000 << std::setw(16) << improved * 1000 << "\n";
   }
   report << "\nAverage of " << BENCH_RUNS << " runs after one untimed warm-up run. Data: out/bench.csv\n";
   std::cerr << "(checksum " << sink << ")\n";
}

// ---------------------------------------------------------------------------------------------
// 3. Collisions
// ---------------------------------------------------------------------------------------------

struct SetStats {
   std::size_t distinctInputs = 0;
   std::size_t distinctHashes = 0;
   std::size_t groups = 0;            // groups of distinct inputs sharing one hash
   std::size_t collidingInputs = 0;   // distinct inputs inside those groups
   std::uint64_t collidingPairs = 0;  // sum of k(k-1)/2 over the groups
};

static SetStats analyzeSet(const std::vector<std::string>& inputs, const std::vector<std::string>& hashes) {
   std::unordered_set<std::string_view> seenInputs;
   std::unordered_map<std::string_view, std::size_t> groupSize;
   for (std::size_t i = 0; i < inputs.size(); ++i) {
      if (seenInputs.insert(inputs[i]).second) ++groupSize[hashes[i]];
   }

   SetStats s;
   s.distinctInputs = seenInputs.size();
   s.distinctHashes = groupSize.size();
   for (const auto& [hash, k] : groupSize) {
      if (k > 1) {
         ++s.groups;
         s.collidingInputs += k;
         s.collidingPairs += static_cast<std::uint64_t>(k) * (k - 1) / 2;
      }
   }
   return s;
}

// Same categories and generator as Experiment/exp2/collision_test.cpp
struct Category {
   std::string name;
   std::vector<std::string> inputs;
};

static std::vector<Category> buildCategories() {
   std::mt19937_64 rng(SEED);
   std::vector<Category> cats;

   {
      Category c{"permutations", {}};
      std::string s = "abcdefgh";
      do {
         c.inputs.push_back(s);
      } while (std::next_permutation(s.begin(), s.end()));
      cats.push_back(std::move(c));
   }
   {
      Category c{"permutations_repeated", {}};
      std::string s = "aabbccdd";
      do {
         c.inputs.push_back(s);
      } while (std::next_permutation(s.begin(), s.end()));
      cats.push_back(std::move(c));
   }
   {
      Category c{"rotations", {}};
      for (int k = 1; k <= 20; ++k) {
         std::string s = randomString(rng, 8 * k);
         for (std::size_t r = 0; r < s.size(); ++r) {
            c.inputs.push_back(s);
            std::rotate(s.begin(), s.begin() + 1, s.end());
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"swap_inside_pair", {}};
      for (int k = 0; k < 20; ++k) {
         std::string base = randomString(rng, 64);
         c.inputs.push_back(base);
         for (int i = 0; i + 1 < 64; i += 2) {
            std::string v = base;
            std::swap(v[i], v[i + 1]);
            c.inputs.push_back(v);
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"swap_across_pair", {}};
      for (int k = 0; k < 20; ++k) {
         std::string base = randomString(rng, 64);
         c.inputs.push_back(base);
         for (int i = 1; i + 1 < 64; i += 2) {
            std::string v = base;
            std::swap(v[i], v[i + 1]);
            c.inputs.push_back(v);
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"repeated_patterns", {}};
      for (const std::string p : {"a", "ab", "abc", "abcd", "0123456789", " "}) {
         for (int len = 1; len <= 300; ++len) {
            std::string s;
            while (static_cast<int>(s.size()) < len) s += p;
            s.resize(len);
            c.inputs.push_back(s);
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"short_inputs", {}};
      for (char x : ALPHABET) c.inputs.push_back(std::string(1, x));
      for (char x : ALPHABET) {
         for (char y : ALPHABET) c.inputs.push_back(std::string{x, y});
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"single_byte_change", {}};
      for (int k = 0; k < 10; ++k) {
         std::string base = randomString(rng, 256);
         c.inputs.push_back(base);
         for (std::size_t i = 0; i < base.size(); ++i) {
            std::string v = base;
            std::size_t pos = ALPHABET.find(v[i]);
            v[i] = ALPHABET[(pos + 1) % ALPHABET.size()];
            c.inputs.push_back(v);
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"same_sum_change", {}};
      for (int k = 0; k < 10; ++k) {
         std::string base = randomString(rng, 64);
         c.inputs.push_back(base);
         for (std::size_t i = 0; i + 1 < base.size(); ++i) {
            if (base[i] == '~' || base[i + 1] == ' ') continue;
            std::string v = base;
            ++v[i];
            --v[i + 1];
            c.inputs.push_back(v);
         }
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"tail_after_128", {}};
      std::string prefix = randomString(rng, 128);
      std::string tail = "abcdef";
      do {
         c.inputs.push_back(prefix + tail);
      } while (std::next_permutation(tail.begin(), tail.end()));
      cats.push_back(std::move(c));
   }
   {
      Category c{"zero_padding", {}};
      for (int k = 0; k < 20; ++k) {
         std::string base = randomString(rng, 5 + k);
         for (int pad = 0; pad <= 8; ++pad) c.inputs.push_back(base + std::string(pad, '\0'));
      }
      cats.push_back(std::move(c));
   }
   {
      Category c{"whitespace", {}};
      for (int k = 0; k < 50; ++k) {
         std::string w = randomString(rng, 3 + k % 10);
         for (const std::string v : {w, " " + w, w + " ", " " + w + " ", "  " + w, w + "  "}) {
            c.inputs.push_back(v);
         }
      }
      cats.push_back(std::move(c));
   }

   return cats;
}

static void runCollisions(std::ostream& report, int pairs) {
   writeTitle(report, "3. Collisions: random inputs (" + std::to_string(pairs) + " pairs per length)");

   report << std::left << std::setw(8) << "Length" << std::right << std::setw(10) << "Distinct";
   for (const auto& h : HASHES) {
      report << "  | " << std::left << std::setw(10) << h.name << std::right << std::setw(11) << "Pair coll."
             << std::setw(10) << "Hashes" << std::setw(13) << "Coll. pairs";
   }
   report << "\n" << std::string(114, '-') << "\n";

   for (int length : LENGTHS) {
      std::cerr << "Collisions, length " << length << "...\n";
      std::mt19937_64 rng(SEED + length);
      std::vector<std::string> inputs;
      inputs.reserve(2 * static_cast<std::size_t>(pairs));
      for (int i = 0; i < pairs; ++i) {
         std::string a = randomString(rng, length);
         std::string b = randomString(rng, length);
         while (b == a) b = randomString(rng, length);
         inputs.push_back(std::move(a));
         inputs.push_back(std::move(b));
      }

      bool first = true;
      for (const auto& h : HASHES) {
         std::vector<std::string> hashes = hashAll(h, inputs);
         std::size_t pairCollisions = 0;
         for (int i = 0; i < pairs; ++i) {
            if (hashes[2 * i] == hashes[2 * i + 1]) ++pairCollisions;
         }
         SetStats s = analyzeSet(inputs, hashes);
         if (first) {
            report << std::left << std::setw(8) << length << std::right << std::setw(10) << s.distinctInputs;
            first = false;
         }
         report << "  | " << std::setw(10) << "" << std::setw(11) << pairCollisions << std::setw(10) << s.distinctHashes
                << std::setw(13) << s.collidingPairs;
      }
      report << "\n";
   }
   report << "\nPair coll.:  pairs whose two inputs got the same hash\n"
          << "Hashes:      distinct hashes among the distinct inputs (equal to Distinct means no collisions)\n"
          << "Coll. pairs: distinct input pairs in the full set that share a hash\n";

   writeTitle(report, "3. Collisions: structured inputs");
   report << std::left << std::setw(24) << "Category" << std::right << std::setw(9) << "Inputs" << std::setw(10) << "Distinct";
   for (const auto& h : HASHES) {
      report << "  | " << std::left << std::setw(10) << h.name << std::right << std::setw(8) << "Hashes"
             << std::setw(14) << "Coll. inputs";
   }
   report << "\n" << std::string(113, '-') << "\n";

   std::cerr << "Collisions, structured inputs...\n";
   for (const auto& cat : buildCategories()) {
      bool first = true;
      for (const auto& h : HASHES) {
         SetStats s = analyzeSet(cat.inputs, hashAll(h, cat.inputs));
         if (first) {
            report << std::left << std::setw(24) << cat.name << std::right << std::setw(9) << cat.inputs.size()
                   << std::setw(10) << s.distinctInputs;
            first = false;
         }
         report << "  | " << std::setw(10) << "" << std::setw(8) << s.distinctHashes << std::setw(14) << s.collidingInputs;
      }
      report << "\n";
   }
   report << "\nCategory descriptions: Experiment/exp2/out/structured_collisions.txt\n";
}

// ---------------------------------------------------------------------------------------------
// 4. Avalanche
// ---------------------------------------------------------------------------------------------

static int hexValue(char c) {
   return (c <= '9') ? c - '0' : c - 'a' + 10;
}

static int bitDifference(const std::string& a, const std::string& b) {
   int bits = 0;
   for (int i = 0; i < HASH_HEX; i += 2) {
      int byteA = hexValue(a[i]) * 16 + hexValue(a[i + 1]);
      int byteB = hexValue(b[i]) * 16 + hexValue(b[i + 1]);
      bits += std::bitset<8>(byteA ^ byteB).count();
   }
   return bits;
}

static int hexDifference(const std::string& a, const std::string& b) {
   int digits = 0;
   for (int i = 0; i < HASH_HEX; ++i) {
      if (a[i] != b[i]) ++digits;
   }
   return digits;
}

struct Stats {
   int count = 0;
   double minimum = std::numeric_limits<double>::infinity();
   double maximum = 0.0;
   double total = 0.0;

   void add(double value) {
      ++count;
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
      total += value;
   }
   double average() const { return total / count; }
};

static void writeAvalancheRow(std::ostream& out, const std::string& label, const Stats& bits, const Stats& hex) {
   out << std::left << std::setw(8) << label << std::right << std::setw(8) << bits.count
       << std::setw(10) << bits.minimum << std::setw(10) << bits.maximum << std::setw(10) << bits.average()
       << std::setw(10) << hex.minimum << std::setw(10) << hex.maximum << std::setw(10) << hex.average() << "\n";
}

static void runAvalanche(std::ostream& report, int pairs, const fs::path& outDir) {
   const int pairsPerLength = pairs / LENGTHS.size();
   writeTitle(report, "4. Avalanche: " + std::to_string(pairsPerLength * LENGTHS.size())
                      + " pairs that differ in one character (" + std::to_string(pairsPerLength) + " per length)");

   std::ofstream csv(outDir / "avalanche.csv");
   csv << "length,original_bit_pct,improved_bit_pct,original_hex_pct,improved_hex_pct\n" << std::setprecision(10);

   const std::size_t n = HASHES.size();
   std::vector<std::vector<Stats>> bitStats(n, std::vector<Stats>(LENGTHS.size())), hexStats = bitStats;
   std::vector<Stats> bitTotal(n), hexTotal(n);

   for (std::size_t l = 0; l < LENGTHS.size(); ++l) {
      int length = LENGTHS[l];
      std::cerr << "Avalanche, length " << length << "...\n";
      std::mt19937_64 rng(SEED + length);

      for (int p = 0; p < pairsPerLength; ++p) {
         std::string a = randomString(rng, length);
         std::string b = a;
         std::size_t pos = randomIndex(rng, length);
         std::size_t oldIndex = ALPHABET.find(a[pos]);
         std::size_t newIndex = randomIndex(rng, ALPHABET.size() - 1);
         if (newIndex >= oldIndex) ++newIndex;
         b[pos] = ALPHABET[newIndex];

         std::vector<double> bitPct(n), hexPct(n);
         for (std::size_t h = 0; h < n; ++h) {
            std::string hashA = HASHES[h].function(a, "");
            std::string hashB = HASHES[h].function(b, "");
            bitPct[h] = 100.0 * bitDifference(hashA, hashB) / HASH_BITS;
            hexPct[h] = 100.0 * hexDifference(hashA, hashB) / HASH_HEX;
            bitStats[h][l].add(bitPct[h]);
            hexStats[h][l].add(hexPct[h]);
            bitTotal[h].add(bitPct[h]);
            hexTotal[h].add(hexPct[h]);
         }
         csv << length << ',' << bitPct[0] << ',' << bitPct[1] << ',' << hexPct[0] << ',' << hexPct[1] << '\n';
      }
   }

   for (std::size_t h = 0; h < n; ++h) {
      report << HASHES[h].name << ":\n";
      report << std::left << std::setw(8) << "" << std::right << std::setw(8) << ""
             << std::setw(30) << "Bit difference (%)" << std::setw(30) << "Hex difference (%)" << "\n";
      report << std::left << std::setw(8) << "Length" << std::right << std::setw(8) << "Pairs"
             << std::setw(10) << "Min" << std::setw(10) << "Max" << std::setw(10) << "Avg"
             << std::setw(10) << "Min" << std::setw(10) << "Max" << std::setw(10) << "Avg" << "\n";
      report << std::string(76, '-') << "\n";
      for (std::size_t l = 0; l < LENGTHS.size(); ++l) {
         writeAvalancheRow(report, std::to_string(LENGTHS[l]), bitStats[h][l], hexStats[h][l]);
      }
      report << std::string(76, '-') << "\n";
      writeAvalancheRow(report, "All", bitTotal[h], hexTotal[h]);
      report << "\n";
   }
   report << "Reference averages for independent, uniform outputs: 50% of bits, 93.75% of hex digits.\n";
   report << "Per-pair values: out/avalanche.csv\n";
}

// ---------------------------------------------------------------------------------------------
// 5. Brute force
// ---------------------------------------------------------------------------------------------

struct AttackResult {
   std::uint64_t attemptsToFirst = 0;
   double seconds = 0.0;
   std::vector<std::string> matches;
};

static AttackResult attack(const HashImpl& h, const std::string& targetHash, const std::vector<std::string>& candidates,
                           const std::string& saltHex) {
   AttackResult r;
   auto start = std::chrono::steady_clock::now();
   for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (h.function(candidates[i], saltHex) == targetHash) {
         if (r.matches.empty()) r.attemptsToFirst = i + 1;
         r.matches.push_back(candidates[i]);
      }
   }
   r.seconds = secondsSince(start);
   return r;
}

static void runBruteForce(std::ostream& report) {
   std::cerr << "Brute force...\n";

   // Same target and salt as Experiment/exp3/brute_force.cpp
   std::mt19937_64 rng(SEED);
   std::vector<std::string> candidates;
   for (int i = 0; i <= 9999; ++i) {
      std::ostringstream ss;
      ss << std::setw(4) << std::setfill('0') << i;
      candidates.push_back(ss.str());
   }
   const std::string target = candidates[rng() % candidates.size()];
   std::ostringstream salt;
   for (int i = 0; i < SALT_BYTES; ++i) {
      salt << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rng() & 0xFF);
   }

   writeTitle(report, "5. Brute force: " + std::to_string(candidates.size()) + " candidates \"0000\" to \"9999\"");
   report << "Chosen input (hidden from the attack): \"" << target << "\", public salt " << salt.str() << "\n\n";
   report << std::left << std::setw(10) << "Hash" << std::setw(10) << "Salt" << std::right
          << std::setw(16) << "Attempts (all)" << std::setw(18) << "Until 1st match" << std::setw(14) << "Time ms"
          << "   Matches\n";
   report << std::string(80, '-') << "\n";

   for (const auto& h : HASHES) {
      for (const std::string& s : {std::string(), salt.str()}) {
         AttackResult r = attack(h, h.function(target, s), candidates, s);
         std::string matches;
         for (const auto& m : r.matches) matches += (matches.empty() ? "\"" : ", \"") + m + "\"";
         report << std::left << std::setw(10) << h.name << std::setw(10) << (s.empty() ? "no" : "public") << std::right
                << std::setw(16) << candidates.size() << std::setw(18) << r.attemptsToFirst
                << std::setw(14) << std::setprecision(3) << r.seconds * 1000 << "   " << matches << "\n";
      }
   }
}

// ---------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
   fs::path baseDir = (argc > 1) ? fs::path(argv[1]) : fs::absolute(argv[0]).lexically_normal().parent_path();
   int pairs = (argc > 2) ? std::stoi(argv[2]) : DEFAULT_PAIRS;
   fs::path exp1Dir = baseDir.parent_path() / "exp1";
   fs::path outDir = baseDir / "out";
   fs::create_directories(outDir);

   // Untimed warm-up runs that load the image for both hashes
   for (const auto& h : HASHES) h.function("warm-up", "");

   std::ofstream report(baseDir / "exp4_results.txt");
   report << std::fixed << std::setprecision(2);
   report << "Original: hash.hpp, improved: hash_v2.hpp\n";
   report << "Inputs and seeds are the same as in experiments 1-3 (std::mt19937_64, seed " << SEED << ")\n";

   std::cerr << "Correctness...\n";
   runCorrectness(report, exp1Dir / "gen");
   std::cerr << "Speed...\n";
   runSpeed(report, exp1Dir / "gen_konst", outDir);
   report << std::setprecision(2);
   runCollisions(report, pairs);
   runAvalanche(report, pairs, outDir);
   runBruteForce(report);

   std::cerr << "Done. Results: " << (baseDir / "exp4_results.txt").string() << "\n";
   return 0;
}
