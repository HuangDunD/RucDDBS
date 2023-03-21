#include <string>

#include "Option.h"
#include "KVStore.h"

KVStore::KVStore(const std::string &dir) : diskstorage_(dir){
    
}

KVStore::~KVStore() {
	if(!memtable_.empty()){
        diskstorage_.add(memtable_);
    }
}

void KVStore::put(uint64_t key, const std::string &value) {
    memtable_.put(key, value);
    if(memtable_.space() > Option::SSTABLE_SPACE){
        diskstorage_.add(memtable_);
        memtable_.clear();
    }
}

std::string KVStore::get(uint64_t key) {
    if(memtable_.contains(key)){
        return memtable_.get(key);
    }
    auto result = diskstorage_.search(key);
    return result.second;
}

// TODO design delete
bool KVStore::del(uint64_t key) {
    return false;
}

void KVStore::reset() {
    // memtable_.clear();
    // diskstorage_.clear();
}