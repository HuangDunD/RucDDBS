#ifndef STORAGE_LEVEL_ZERO
#define STORAGE_LEVEL_ZERO

#include <string>
#include <cstdint>

#include "SkipList.h"
#include "TableCache.h"
#include "BlockCache.h"
#include "SSTable.h"

class LevelZero {
 public:
   explicit LevelZero(const std::string &dir, TableCache* table_cache, BlockCache *block_cache);
   void add(const SkipList &memtable, uint64_t &no);
   std::pair<bool, std::string> search(uint64_t key);
 private:
   std::string dir_;
   std::vector<SSTable> ssts_;
   uint64_t size_;
   uint64_t num_entries_;
   uint64_t num_bytes_;
   TableCache *table_cache_;
   BlockCache *block_cache_;

   void save_meta() const;
};

#endif