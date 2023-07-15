/*
* Inmemory为在内存中的存储代码, 用于在未完成存储层全部功能之前, 对于事务部分和日志恢复部分的测试工作
* 简单采用一个Hash表来记录所有的kv键值对
*/
#pragma once
#include <unordered_map>
#include "Transaction.h"
#include "log_manager.h"

class KVStore{
 public:
    explicit KVStore(LogManager* log_manager){log_manager_ = log_manager;};
    ~KVStore(){};
    // put(key, value)
    void put(const std::string &key, const std::string &value){
        memtable_[key] = value;
    };
    void put(const std::string &key, const std::string &value, Transaction *txn, bool add_writeset = true){
        if(enable_logging){
            //写Put日志
            LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::INSERT,
                        key.size(), key.c_str() ,value.size(), value.c_str());
            auto lsn = log_manager_->AppendLogRecord(record);
            txn->set_prev_lsn(lsn);
        }
        //add write record into write set.
        if(add_writeset){
            WriteRecord wr = WriteRecord(key, WType::INSERT_TUPLE);
            txn->get_write_set()->push_back(wr);
        }
        memtable_[key] = value;
    }

    // value = get(key)
    std::string get(const std::string &key){
        // if(memtable_.count(key) == 0){
        //     return "";
        // }
        // return memtable_[key];
        auto iter = memtable_.find(key);
        if (iter != memtable_.end()) {
            // 找到了元素
            std::string value = iter->second;
            return value;
        } else {
            // 没有找到元素
            return "";
        }
    }

    // del(key)
    bool del(const std::string &key){
        if(memtable_.count(key) == 0){
            return false;
        }
        memtable_.erase(key);
        return true;
    }
    bool del(const std::string &key, Transaction *txn,  bool add_writeset = true){
        // if(memtable_.count(key) == 0){
        //     return false;
        // }
        auto iter = memtable_.find(key);
        if (iter == memtable_.end()) {
            return false;
        }
        if(enable_logging){
            //写Put日志
            LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::DELETE,
                        key.size(), key.c_str() ,memtable_[key].size(), memtable_[key].c_str());
            auto lsn = log_manager_->AppendLogRecord(record);
            txn->set_prev_lsn(lsn);
        }
         //add write record into write set.
        if(add_writeset){
            WriteRecord wr = WriteRecord(key, memtable_[key], WType::DELETE_TUPLE);
            txn->get_write_set()->push_back(wr);
        }
        memtable_.erase(key);
        return true;
    }
    
    // clear memtable and disk
    void reset(){
        memtable_.clear();
    }
 private:
    std::unordered_map<std::string, std::string> memtable_;
    LogManager *log_manager_;
};