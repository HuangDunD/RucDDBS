#ifndef STORAGE_TABLE_CACHE_H
#define STORAGE_TABLE_CACHE_H

#include <fstream>
#include <list>
#include <unordered_map>

#include <iostream>

#include "SSTable.h"
#include "SSTableId.h"

class TableCache {
public:
   TableCache() = default;
   ~TableCache();
   // std::ifstream* open(SSTableId no);

   SSTable *open(SSTableId no);

   void close(SSTableId no);
private:

struct file_and_table {
   std::ifstream *ifs_;
   SSTable *sstable_;
};
   std::list<std::pair<uint64_t, file_and_table>> lists_;
   std::unordered_map<uint64_t, std::list<std::pair<uint64_t, file_and_table>>::iterator> map_;
};

#endif