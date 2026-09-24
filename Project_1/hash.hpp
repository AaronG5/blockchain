#pragma once

#include <string>
#include <vector>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBI_ONLY_PNG

using namespace std;

struct Pixel {
   unsigned char r;
   unsigned char g;
   unsigned char b; 
};

// Helper functions
inline vector<Pixel> loadRawImagePixels(int* imageWidth, int* imageHeight);
inline string decToHexStr(const int& decimal);
inline int getPixelIndex(const int& i, const int& contentSize, const vector<int>& inputContent, 
                         const int& inputSum, const int& imageWidth, const int& imageHeight);

// Main hashing function
inline string hashFunction(const string& input) {
   int imageWidth, imageHeight;
   string hash = "";

   // Step 1
   const vector<Pixel> pixels = loadRawImagePixels(&imageWidth, &imageHeight);

   // Step 2
   const vector<int> inputContent(input.begin(), input.end());
   const int contentSize = inputContent.size();

   // Step 3
   int inputSum = 0; // Used to offset X and Y starting point
   for(int val : inputContent) {
      inputSum += val;
   }

   // Step 4
   for(int i = 0; i < contentSize; i += 4) {
      auto idx = getPixelIndex(i, contentSize, inputContent, inputSum, imageWidth, imageHeight);
      Pixel tempPixel = pixels[idx];

      // Steps 5 and 6
      hash += decToHexStr((tempPixel.r ^ tempPixel.g ^ tempPixel.b));
   }
}

inline vector<Pixel> loadRawImagePixels(int* imageWidth, int* imageHeight) {
   const char* FILENAME = "image.png";
   int channels;
   unsigned char* pixelData = stbi_load(FILENAME, imageWidth, imageHeight, &channels, 3);

   if(!pixelData) {
      throw runtime_error("Failed to load image");
   }

   vector<Pixel> pixels((*imageWidth) * (*imageHeight));
   
   for(int y = 0; y < (*imageHeight); ++y) {
      for(int x = 0; x < (*imageWidth); ++x) {
         int index = (y * (*imageWidth) + x) * channels;
         pixels[y * (*imageWidth) + x] = {
            pixelData[index],     // R
            pixelData[index + 1], // G
            pixelData[index + 2], // B
         };
      }
   }
   stbi_image_free(pixelData);
   return pixels;
}

inline string decToHexStr(const int& decimal) {
   stringstream ss;
   ss << hex << setw(2) << setfill('0') << decimal;
   return ss.str();
}

inline int getPixelIndex(const int& i, const int& contentSize, const vector<int>& inputContent, 
                         const int& inputSum, const int& imageWidth, const int& imageHeight) {
   int x1 = inputContent[i];
   int x2 = (i + 1 < contentSize) ? inputContent[i + 1] : 0;
   int y1 = (i + 2 < contentSize) ? inputContent[i + 2] : 0;
   int y2 = (i + 3 < contentSize) ? inputContent[i + 3] : 0;

   int x = x1 * x2 + inputSum;
   int y = y1 * y2 + inputSum;

   while(x > imageWidth) { // Might break on coordinate edge?
      x = (x % imageWidth) + (x / imageWidth);
   }
   while(y > imageHeight) {
      y = (y % imageHeight) + (y / imageHeight);
   }

   return y * imageWidth + x;
}