// 用一个数组来当作存储， 只可以用于benchmark测试事务层
#pragma once

#include <stdio.h>
#include "Transaction.h"
#include "log_manager.h"

class KVStore{
 public:
    explicit KVStore(int size, LogManager* log_manager){
        log_manager_ = log_manager;
        this->size = size;
        memtable_ = new std::string[size];
    };
    ~KVStore(){};

    // put(key, value)
    void put(const int &key, const std::string &value){
        memtable_[key] = value;
    };
    void put(const int &key, const std::string &value, Transaction *txn, bool add_writeset = true){
        if(enable_logging){
            //写Put日志
            LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::INSERT,
                        std::to_string(key).size(), std::to_string(key).c_str() ,value.size(), value.c_str());
            auto lsn = log_manager_->AppendLogRecord(record);
            txn->set_prev_lsn(lsn);
        }
        //add write record into write set.
        if(add_writeset){
            WriteRecord wr = WriteRecord(std::to_string(key), WType::INSERT_TUPLE);
            txn->get_write_set()->push_back(wr);
        }
        memtable_[key] = value;
    }

    // value = get(key)
    std::string get(const int &key){
        return memtable_[key];
    }

    // del(key)
    bool del(const int &key){
        memtable_[key] = "";
        return true;
    }
    bool del(const int &key, Transaction *txn,  bool add_writeset = true){
        if(enable_logging){
            //写Put日志
            LogRecord record (txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::DELETE,
                        std::to_string(key).size(), std::to_string(key).c_str() ,memtable_[key].size(), memtable_[key].c_str());
            auto lsn = log_manager_->AppendLogRecord(record);
            txn->set_prev_lsn(lsn);
        }
         //add write record into write set.
        if(add_writeset){
            WriteRecord wr = WriteRecord(std::to_string(key), memtable_[key], WType::DELETE_TUPLE);
            txn->get_write_set()->push_back(wr);
        }
        memtable_[key] = "";
        return true;
    }
    
    // clear memtable and disk
    // void reset(){
    //     memtable_.clear();
    // }
 private:
    std::string* memtable_;
    int size;
    LogManager *log_manager_;
};