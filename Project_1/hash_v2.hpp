#pragma once

// Improved version of hash.hpp. It is still image based: image.png is the only source of "randomness",
// used as a table of 24-bit colours. No outside hash functions, constants or random number generators.
//
// Four walkers stand on pixels of the image. Every 8 bytes of the input move one walker: where it goes next
// depends on those bytes, the colour of the pixel it stands on and the next walker. After the whole input
// (and the salt and the input length) the walkers keep walking for a few rounds, and their final values,
// 4 * 64 bits, are the hash.
//
// Fixes compared to hash.hpp:
//    - every byte of the input is used, not only the first 128
//    - a small change of the input moves the walkers to unrelated pixels, not to neighbouring ones
//    - all 24 bits of a pixel are used (not R ^ G ^ B), and every pixel affects all 256 output bits
//    - the input length is part of the input, so trailing zero bytes change the hash
//    - an empty input has a hash

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

namespace hash_v2 {

using State = std::array<std::uint64_t, 4>;   // the four walkers, 4 * 64 = 256 bits

struct Image {
   std::vector<std::uint32_t> colours;   // 0xRRGGBB of every pixel, row by row
   std::uint64_t positions = 0;          // number of pixels a walker can stand on (a prime, see loadImage)
};

// Helper functions
inline Image loadImage();
inline State startState(const Image& image);
inline void step(State& walkers, const Image& image, std::uint64_t block, std::uint64_t k);
inline std::uint64_t rotateLeft(std::uint64_t value, int bits);
inline std::string decodeSalt(const std::string& saltHex);
inline std::string toHex(const State& walkers);

const int FINAL_ROUNDS = 8;   // extra steps per walker after the input, so every output bit depends on every byte

// Main hashing function
// saltHex is optional: 16 bytes written as 32 hex characters, appended to the input as raw bytes (input || salt)
inline std::string hashFunction(const std::string& input, const std::string& saltHex = "") {
   static const Image image = loadImage();
   static const State start = startState(image);

   State walkers = start;
   std::uint64_t k = 0;

   // 1. Input and salt, 8 bytes per step (the last block is filled up with zero bytes)
   const std::string data = input + decodeSalt(saltHex);
   for(std::size_t i = 0; i < data.size(); i += 8) {
      std::uint64_t block = 0;
      for(std::size_t j = i; j < i + 8 && j < data.size(); ++j) {
         block |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[j])) << (8 * (j - i));
      }
      step(walkers, image, block, k++);
   }

   // 2. The length, so that the zero bytes added to the last block can't be confused with real ones
   step(walkers, image, data.size(), k++);

   // 3. Final rounds: no new bytes, the walkers keep moving each other
   for(int i = 0; i < FINAL_ROUNDS * 4; ++i) {
      step(walkers, image, 0, k++);
   }

   return toHex(walkers);
}

// Moves walker k % 4 by one step:
//    1. read the colour of the pixel it stands on
//    2. mix the block (8 bytes) into it, rotate, add the colour and the next walker
//    3. the next walker is changed by the result, so it moves somewhere else on its own step
// The position is the walker modulo a prime, so all 64 bits decide where it stands, not only the low ones.
inline void step(State& walkers, const Image& image, std::uint64_t block, std::uint64_t k) {
   std::uint64_t& current = walkers[k % 4];
   std::uint64_t& next = walkers[(k + 1) % 4];

   std::uint64_t colour = image.colours[current % image.positions];
   current = rotateLeft(current ^ block, 13) + (colour << 20) + colour + next;
   next ^= rotateLeft(current, 37);
}

inline std::uint64_t rotateLeft(std::uint64_t value, int bits) {
   return (value << bits) | (value >> (64 - bits));
}

inline Image loadImage() {
   const char* FILENAME = "image.png";
   const int CHANNELS = 3;
   int chnl, imageWidth, imageHeight;

   unsigned char* pixelData = stbi_load(FILENAME, &imageWidth, &imageHeight, &chnl, CHANNELS);
   if(!pixelData) {
      throw std::runtime_error("Failed to load image");
   }

   Image image;
   std::uint64_t pixelCount = static_cast<std::uint64_t>(imageWidth) * imageHeight;
   image.colours.resize(pixelCount);
   for(std::uint64_t i = 0; i < pixelCount; ++i) {
      image.colours[i] = (pixelData[i * CHANNELS] << 16) | (pixelData[i * CHANNELS + 1] << 8) | pixelData[i * CHANNELS + 2];
   }
   stbi_image_free(pixelData);

   if(pixelCount < 2) {
      throw std::runtime_error("Image is too small");
   }

   // Largest prime that is not bigger than the pixel count. Taking a number modulo a prime depends on all
   // of its bits; modulo 1536 * 2048 = 3 * 2^20 it would mostly depend on the low 20 bits.
   std::uint64_t prime = pixelCount;
   auto isPrime = [](std::uint64_t n) {
      if(n < 2) return false;
      for(std::uint64_t d = 2; d * d <= n; ++d) {
         if(n % d == 0) return false;
      }
      return true;
   };
   while(!isPrime(prime)) {
      --prime;
   }
   image.positions = prime;
   return image;
}

// Starting values come from the image: walker i is built from three pixels on the image diagonal,
// then the walkers take a few steps so they no longer look like colours
inline State startState(const Image& image) {
   State walkers{};
   std::uint64_t pixelCount = image.colours.size();
   for(int i = 0; i < 4; ++i) {
      std::uint64_t at = pixelCount / 5 * (i + 1);
      walkers[i] = (static_cast<std::uint64_t>(image.colours[at]) << 40)
                 ^ (static_cast<std::uint64_t>(image.colours[at + 1]) << 20)
                 ^ image.colours[at + 2];
   }
   for(int i = 0; i < FINAL_ROUNDS * 4; ++i) {
      step(walkers, image, 0, i);
   }
   return walkers;
}

inline std::string decodeSalt(const std::string& saltHex) {
   const std::size_t SALT_BYTES = 16;
   if(saltHex.empty()) {
      return "";
   }
   if(saltHex.size() != SALT_BYTES * 2 || saltHex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
      throw std::runtime_error("Salt must be 32 hex characters (16 bytes)");
   }

   std::string salt;
   for(std::size_t i = 0; i < saltHex.size(); i += 2) {
      salt += static_cast<char>(std::stoi(saltHex.substr(i, 2), nullptr, 16));
   }
   return salt;
}

// Each walker becomes 16 hex characters, 64 in total
inline std::string toHex(const State& walkers) {
   const char* DIGITS = "0123456789abcdef";
   std::string hash;
   for(std::uint64_t walker : walkers) {
      for(int shift = 60; shift >= 0; shift -= 4) {
         hash += DIGITS[(walker >> shift) & 0xF];
      }
   }
   return hash;
}

} // namespace hash_v2
