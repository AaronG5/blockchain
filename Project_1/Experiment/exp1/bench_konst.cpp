#include <stdexcept>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

#include "../../hash.hpp"

namespace fs = std::filesystem;

struct Result {
   std::string filename;
   std::uintmax_t sizeBytes;
   double timeSeconds;
   double minimum;
   double maximum;
};

static int fileNumber(const fs::path& p) {
   std::string stem = p.stem().string();
   size_t pos = stem.find('_');
   if (pos == std::string::npos) return 0;
   return std::stoi(stem.substr(pos + 1));
}

static std::string readFile(const fs::path& p) {
   std::ifstream f(p, std::ios::binary);
   std::stringstream ss;
   ss << f.rdbuf();
   return ss.str();
}

int main(int argc, char* argv[]) {
   fs::path baseDir = (argc > 1) ? fs::path(argv[1]) : fs::absolute(argv[0]).parent_path();
   fs::path genKonst = baseDir / "gen_konst";
   fs::path outKonst = baseDir / "out_konst";
   fs::create_directories(outKonst);

   std::vector<fs::path> files;
   for (const auto& entry : fs::directory_iterator(genKonst)) {
      if (entry.is_regular_file() && entry.path().extension() == ".txt") {
         files.push_back(entry.path());
      }
   }
   std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
      return fileNumber(a) < fileNumber(b);
   });

   const int n = 10;
   std::vector<Result> results;
   size_t sink = 0;

   for (const auto& file : files) {
      std::string content = readFile(file);

      double total = 0.0;
      double minimum = std::numeric_limits<double>::infinity();
      double maximum = 0.0;

      // Untimed warm-up run
      sink += hashFunction(content).size();

      for (int i = 0; i < n; ++i) {
         auto start = std::chrono::steady_clock::now();

         std::string h = hashFunction(content);

         auto end = std::chrono::steady_clock::now();
         double elapsed = std::chrono::duration<double>(end - start).count();

         sink += h.size();
         total += elapsed;
         minimum = std::min(minimum, elapsed);
         maximum = std::max(maximum, elapsed);
      }

      results.push_back({
         file.filename().string(),
         fs::file_size(file),
         total / n,
         minimum,
         maximum
      });
   }

   std::ofstream out(outKonst / "bench_konst.csv");
   out << "filename,size_bytes,time_seconds,min,max\n";
   out << std::setprecision(12);
   for (const auto& r : results) {
      out << r.filename << ',' << r.sizeBytes << ','
         << r.timeSeconds << ',' << r.minimum << ',' << r.maximum << '\n';
   }

   std::cerr << "Done (" << results.size() << " files, checksum " << sink << ")\n";
   return 0;
}