#ifndef STORAGE_KV_STORE_H
#define STORAGE_KV_STORE_H

#include <cstdint>
#include <string>

#include "SkipList.h"
#include "DiskStorage.h"
#include "Transaction.h"
#include "log_manager.h"
 
class KVStore{
public:
   explicit KVStore(const std::string &dir);
   
   KVStore(const KVStore &) = delete;
   KVStore &operator=(const KVStore &) = delete;

   ~KVStore();

//  // put(key, value)
//  void put(uint64_t key, const std::string &value, Transaction *txn);
//  // value = get(key)
//  std::string get(uint64_t key, Transaction *txn);
//  // del(key)
//  bool del(uint64_t key, Transaction *txn);

   // put(key, value)
   void put(const std::string & key, const std::string &value, Transaction *txn);
   // value = get(key)
   std::pair<bool, std::string> get(const std::string & key, Transaction *txn);
   // del(key)
   bool del(const std::string & key, Transaction *txn);
   
   // clear memtable and disk
   void reset();
   // flush memtable to disk 
   void flush();

private:
   SkipList memtable_;
   DiskStorage diskstorage_;
   LogManager* log_manager_;
};

#endif