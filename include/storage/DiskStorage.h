#ifndef STORAGE_DISK_STORAGE
#define STORAGE_DISK_STORAGE

#include <string>
#include <cstdint>

#include "SkipList.h"
#include "LevelZero.h" 
#include "BlockCache.h"
#include "TableCache.h"

class DiskStorage{
 public:
    explicit DiskStorage(const std::string &dir);
    void add(const SkipList &memtable);
    std::pair<bool, std::string> search(uint64_t key);
 private:
    std::string dir_;
    uint64_t no_;
    BlockCache block_cache_;
    TableCache table_cache_;
    LevelZero level0_;
    
    void read_meta();
    void save_meta() const;
};

#endif