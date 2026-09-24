#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "hash.hpp"

using namespace std;

const int MAX_INPUT_LENGTH = 128;

enum class Mode { TEXT, FILE_ };

string modeName(Mode mode) {
   return (mode == Mode::TEXT) ? "Text" : "File";
}

// Reads an entire text file into a single string, stripping trailing newline characters.
bool readFileContent(const string& filename, string& outContent) {
   ifstream file(filename, ios::binary);
   if(!file.is_open()) {
      return false;
   }

   stringstream buffer;
   buffer << file.rdbuf();
   outContent = buffer.str();

   return true;
}

void appendHashToFile(const string& hash) {
   ofstream outFile("hashes.txt", ios::app);
   if(outFile.is_open()) {
      outFile << hash << "\n";
   } else {
      cout << "Warning: could not write to hashes.txt" << endl;
   }
}

void printMenu(Mode currentMode) {
   cout << "\n----------------------------------------" << endl;
   cout << "Current mode: " << modeName(currentMode) << endl;
   cout << "----------------------------------------" << endl;
   cout << "[1] Hash input" << endl;
   cout << "[2] Switch mode (currently: " << modeName(currentMode) << ")" << endl;
   cout << "[3] Exit" << endl;
   cout << "Choice: ";
}

int main(const int argc, const char* argv[]) {
   cout << "Welcome to the image-based hashing function." << endl;

   Mode currentMode = Mode::TEXT;
   bool running = true;

   while(running) {
      printMenu(currentMode);

      string choice;
      getline(cin, choice);

      if(choice == "1") {
         string input;

         if(currentMode == Mode::TEXT) {
            cout << "Enter text to hash: ";
            getline(cin, input);
         } else {
            cout << "Enter filename (.txt): ";
            string filename;
            getline(cin, filename);

            if(filename.size() < 4 || filename.substr(filename.size() - 4) != ".txt") {
               cout << "Error: filename must have a .txt extension." << endl;
               continue;
            }

            if(!readFileContent(filename, input)) {
               cout << "Error: could not open file \"" << filename << "\"." << endl;
               continue;
            }
         }

         if(input.empty()) {
            cout << "Error: input is empty." << endl;
            continue;
         }

         if(static_cast<int>(input.size()) > MAX_INPUT_LENGTH) {
            cout << "Error: input exceeds " << MAX_INPUT_LENGTH << " character limit "
                 << "(got " << input.size() << ")." << endl;
            continue;
         }

         try {
            string hash = hashFunction(input);
            cout << "Hash: " << hash << endl;
            appendHashToFile(hash);
            cout << "Hash appended to hashes.txt" << endl;
         } catch(const exception& e) {
            cout << "Error: " << e.what() << endl;
         }

      } else if(choice == "2") {
         currentMode = (currentMode == Mode::TEXT) ? Mode::FILE_ : Mode::TEXT;
         cout << "Switched to " << modeName(currentMode) << " mode." << endl;
      } else if(choice == "3") {
         running = false;
      } else {
         cout << "Invalid choice, please try again." << endl;
      }
   }

   cout << "Goodbye." << endl;
   return 0;
}