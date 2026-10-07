#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

struct Pixel {
   unsigned char r;
   unsigned char g;
   unsigned char b; 
};

struct ImageData {
   vector<Pixel> pixels;
   int imageWidth = 0;
   int imageHeight = 0;
};

// Helper functions
inline ImageData loadRawImage();
inline string decToHexStr(const int& decimal);
inline string decodeSalt(const string& saltHex);
inline int getPixelIndex(const int& i, const int& contentSize, const vector<int>& inputContent, 
                         const int& inputSum, const int& outputIndex, const int& imageWidth, const int& imageHeight);

// Main hashing function
// saltHex is optional: 16 bytes written as 32 hex characters, appended to the input as raw bytes (input || salt)
inline string hashFunction(const string& input, const string& saltHex = "") {
   const int HASH_SIZE = 64;
   string hash = "";

   // Step 1
   static ImageData cachedImage = loadRawImage();

   // Step 2
   vector<int> inputContent;
   for(unsigned char c : input) inputContent.push_back(c);
   for(unsigned char c : decodeSalt(saltHex)) inputContent.push_back(c);
   const int contentSize = inputContent.size();
   
   if(contentSize <= 0) {
      throw runtime_error("Empty input");
   }

   // Step 3
   int inputSum = 0; // Used to offset X and Y starting point
   for(int val : inputContent) {
      inputSum += val;
   }
   inputSum += contentSize * 256;

   // Loop used for step 7
   for(int i = 0;; ++i) {
      int shift = i % contentSize;
      rotate(inputContent.begin(), inputContent.end() - shift, inputContent.end());

      // Step 4
      for(int j = 0; j < contentSize; j += 4) {
         int outputIndex = hash.size() / 2;
         auto idx = getPixelIndex(j, contentSize, inputContent, inputSum, outputIndex, cachedImage.imageWidth, cachedImage.imageHeight);
         Pixel tempPixel = cachedImage.pixels[idx];
   
         // Steps 5 and 6
         hash += decToHexStr((tempPixel.r ^ tempPixel.g ^ tempPixel.b));
         if(hash.size() >= HASH_SIZE) {
            break;
         }
      }

      // Steps 8 and 9
      if(hash.size() > HASH_SIZE) {
         hash.resize(HASH_SIZE);
         return hash;
      }
      if(hash.size() == HASH_SIZE) {
         return hash;
      }
   }
}

inline ImageData loadRawImage() {
   const char* FILENAME = "image.png";
   const int CHANNELS = 3;
   int chnl, imageWidth, imageHeight;

   unsigned char* pixelData = stbi_load(FILENAME, &imageWidth, &imageHeight, &chnl, CHANNELS);

   if(!pixelData) {
      throw runtime_error("Failed to load image");
   }

   ImageData imageData;
   imageData.imageWidth = imageWidth;
   imageData.imageHeight = imageHeight;
   imageData.pixels.resize(imageWidth * imageHeight);
   
   for(int y = 0; y < imageHeight; ++y) {
      for(int x = 0; x < imageWidth; ++x) {
         int index = (y * imageWidth + x) * CHANNELS;
         
         imageData.pixels[y * imageWidth + x] = {
            pixelData[index],     // R
            pixelData[index + 1], // G
            pixelData[index + 2], // B
         };
      }
   }
   stbi_image_free(pixelData);
   return imageData;
}

inline string decToHexStr(const int& decimal) {
   stringstream ss;
   ss << hex << setw(2) << setfill('0') << decimal;
   return ss.str();
}

inline string decodeSalt(const string& saltHex) {
   const int SALT_BYTES = 16;
   if(saltHex.empty()) {
      return "";
   }
   if(saltHex.size() != SALT_BYTES * 2 || saltHex.find_first_not_of("0123456789abcdefABCDEF") != string::npos) {
      throw runtime_error("Salt must be 32 hex characters (16 bytes)");
   }

   string salt;
   for(int i = 0; i < saltHex.size(); i += 2) {
      salt += static_cast<char>(stoi(saltHex.substr(i, 2), nullptr, 16));
   }
   return salt;
}

inline int getPixelIndex(const int& i, const int& contentSize, const vector<int>& inputContent, 
                         const int& inputSum, const int& outputIndex, const int& imageWidth, const int& imageHeight) {
   int x1 = inputContent[i];
   int x2 = (i + 1 < contentSize) ? inputContent[i + 1] : 0;
   int y1 = (i + 2 < contentSize) ? inputContent[i + 2] : 0;
   int y2 = (i + 3 < contentSize) ? inputContent[i + 3] : 0;

   int x = x1 * 256 + x2 + inputSum + outputIndex * 7;
   int y = y1 * 256 + y2 + inputSum + outputIndex * 13;

   while(x >= imageWidth) {
      x = (x % imageWidth) + (x / imageWidth);
   }
   while(y >= imageHeight) {
      y = (y % imageHeight) + (y / imageHeight);
   }

   return y * imageWidth + x;
}