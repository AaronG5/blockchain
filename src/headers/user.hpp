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

   const std::string& getUserName() const {
      return username;
   }
   
   const std::string& getPublicKey() const {
      return publicKey;
   }

};