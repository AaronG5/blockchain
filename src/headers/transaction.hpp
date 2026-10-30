#pragma once

#include <string>
#include <cstdint>

class Transaction {
private: 
   std::string sender;
   std::string receiver;
   std::uint64_t amount;
   std::uint64_t sequence;
   std::string id;
};