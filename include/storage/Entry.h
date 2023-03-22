#ifndef STORAGE_ENTRY_H
#define STORAGE_ENTRY_H

#include <cstdint>
#include <string>

struct Entry {
   uint64_t key_;
   std::string value_;

   Entry(uint64_t key, const std::string &value);
   ~Entry() = default;
};

#endif