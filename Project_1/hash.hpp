#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBI_ONLY_PNG

using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::runtime_error;

struct Pixel {
   unsigned char r;
   unsigned char g;
   unsigned char b; 
};

inline vector<Pixel> loadRawImagePixels(int* imageWidth, int* imageHeight);


inline string hashFunction(const string& input) {
   int imageWidth, imageHeight;

   // Step 1
   const vector<Pixel> pixels = loadRawImagePixels(&imageWidth, &imageHeight);

   // Step 2
   const vector<int> inputContent(input.begin(), input.end());
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
