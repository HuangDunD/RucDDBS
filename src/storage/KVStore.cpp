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

// put(key, value)
void KVStore::put(const std::string & key, const std::string &value, Transaction *txn){
    if(enable_logging){
        //写Put日志
        LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::INSERT,
                    key.size(), key.c_str() ,value.size(), value.c_str());
        auto lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
    }

    // TODO
    // memtable_.put(key, value);
    // if(memtable_.space() > Option::SSTABLE_SPACE){
    //     diskstorage_.add(memtable_);
    //     memtable_.clear();
    // }
}

// value = get(key)
std::string KVStore::get(const std::string & key, Transaction *txn){
    
}

// del(key)
// bool KVStore::del(const std::string & key, Transaction *txn){
//     if(memtable_.contains(key)){
//         if(enable_logging){
//             //写Put日志
//             LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::DELETE,
//                         key.size(), key.c_str() ,memtable_.get(key).size(), memtable_.get(key).c_str());
//             auto lsn = log_manager_->AppendLogRecord(record);
//             txn->set_prev_lsn(lsn);
//         }
//         memtable_.del(key);
//         return true;
//     }
//     auto result = diskstorage_.search(key);
//     if(result.first){
//         if(enable_logging){
//             //写Put日志
//             LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::DELETE,
//                         key.size(), key.c_str() ,result.second.size(), result.second.c_str());
//             auto lsn = log_manager_->AppendLogRecord(record);
//             txn->set_prev_lsn(lsn);
//         }
//         memtable_.put(key, "");
//         return true;
//     }else{
//         return false;
//     }
// }

void KVStore::put(uint64_t key, const std::string &value, Transaction *txn) {
    memtable_.put(key, value);
    if(memtable_.space() > Option::SSTABLE_SPACE){
        diskstorage_.add(memtable_);
        memtable_.clear();
    }
}

std::string KVStore::get(uint64_t key, Transaction *txn) {

    if(memtable_.contains(key)){
        return memtable_.get(key);
    }
    auto result = diskstorage_.search(key);
    return result.second;
}

// TODO design delete
bool KVStore::del(uint64_t key, Transaction *txn) {
    return false;
}

void KVStore::reset() {
    // memtable_.clear();
    // diskstorage_.clear();
}