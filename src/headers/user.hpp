#pragma once

#include <string>
#include <vector>

class User {
private:
   std::string username;
   std::string publicKey;

public:
   User(std::string username, std::string publicKey) 
      : username(username), publicKey(publicKey) {}

   std::string getUserName() {
      return username;
   }
   
   std::string getPublicKey() {
      return publicKey;
   }

};