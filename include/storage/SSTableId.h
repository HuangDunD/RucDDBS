#pragma once

#include <string>
#include <cstdint>

struct SSTableId{
   std::string dir_;
   uint64_t no_;  

   SSTableId() = default;
   
   SSTableId(const std::string &dir, uint64_t no);

   SSTableId(const SSTableId & sst);

   ~SSTableId() =  default;
   
   std::string name() const;

   void EncodeInto(std::string &s) const;

   void DecodeFrom(const std::string &s);
};