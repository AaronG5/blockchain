#pragma once

#include <string>
#include <vector>
#include <cstdint>

class Block {
   struct Header {
      std::string prevHash;
      std::int64_t timestamp;
      std::uint8_t version;
      std::string txRoot;
      std::uint64_t nonce;
      std::uint32_t difficulty;
   };

   Header header;
   std::vector<int> transactions; // int as placeholder for Transaction type
};