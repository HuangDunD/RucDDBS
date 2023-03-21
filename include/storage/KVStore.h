#ifndef STORAGE_KV_STORE_H
#define STORAGE_KV_STORE_H

#include <cstdint>
#include <string>

#include "SkipList.h"
#include "DiskStorage.h"

class KVStore{
 public:
    explicit KVStore(const std::string &dir);
    ~KVStore();
    // put(key, value)
    void put(uint64_t key, const std::string &value);
    // value = get(key)
    std::string get(uint64_t key);
    // del(key)
    bool del(uint64_t key);
    // clear memtable and disk
    void reset();
 private:
    SkipList memtable_;
    DiskStorage diskstorage_;
};

#endif