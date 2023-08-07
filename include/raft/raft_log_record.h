/** 
 * author: hcy
 * 
 * 该文件定义了raft log记录。它衔接了事务和raft，当事务准备/事务提交时，
 * 需要根据事务的写集生成raft log，包括了数据的写入日志和事务的状态日志。
 * 由raft leader发送给各个follower，收到返回结果后，事务才可以向协调者
 * 返回协调结果/提交结果。
 * 
 * 注意这里的raft log与redo/undo log的区别，redo/undo log是随着读写操
 * 作的运行而刷入redo/undo log缓冲区的，当事务准备/提交/回滚时需要强制
 * 刷盘；同时当memtable刷盘时，同样要保证memtable中的最后一条读写操作
 * 日志已经刷入缓冲区，从而实现WAL。
 * 
 * 而raft log是在事务准备/事务提交时一次性生成的，它用于在数据在多副本
 * 之间复制。它实现了即使在一个事务prepared之后宕机，由于可以切换到其他
 * 的副本，该事务同样可以正常提交。其没有必要随着读写操作而在多副本之间
 * 同步，而只需要在准备阶段批量同步。同时，在内容上，它是对redo log的封
 * 装。follower副本收到raft log之后会重做(Redo)它们，并且在这个过程中，
 * 生成Redo/Undo Log。
 * 
**/ 

#pragma once
#include <cstdint>
#include <string>
#include <assert.h>
#include <string.h>
#include <iostream>
#include "Transaction.h"
#include "transaction_manager.h"

using lsn_t = int32_t;
using txn_id_t = uint64_t;

enum class RedoRecordType: std::int32_t { INVALID = 0, CREATE_TABLE, DROP_TABLE, INSERT, 
                                 DELETE, BEGIN, COMMIT, ABORT, PREPARED};

class RedoRecord
{
public:
    static constexpr int RedoRecord_HeadSize = 8;

    RedoRecord() = default;

    // use for txn begin/prepare/abort
    RedoRecord(RedoRecordType log_type): log_type_(log_type){
        assert(log_type == RedoRecordType::BEGIN || log_type == RedoRecordType::COMMIT || log_type == RedoRecordType::ABORT || log_type == RedoRecordType::PREPARED);
        size_ = RedoRecord_HeadSize;
    };

    // use for put and del
    RedoRecord(RedoRecordType log_type, uint32_t key_size, const char* key, uint32_t value_size, const char* value)
        : log_type_(log_type), key_size_(key_size), value_size_(value_size){
            size_ = RedoRecord_HeadSize + sizeof(uint32_t) + key_size_ + sizeof(uint32_t) + value_size_;
    };

    int32_t size_;
    RedoRecordType log_type_{RedoRecordType::INVALID};
    uint32_t key_size_;
    const char* key_;
    uint32_t value_size_;
    const char* value_;
};

class RaftLogRecord {

public:
    RaftLogRecord() = default;
    ~RaftLogRecord(){
        delete[] redo_record_;
    }

    static constexpr int RAFTLOG_HEADER_SIZE = 12;
    
    RaftLogRecord(txn_id_t txn_id, std::shared_ptr<std::deque<WriteRecord>> write_set_, bool append_txn_prepared, bool append_txn_commit)
        : txn_id_(txn_id) {
            
            assert((append_txn_commit ^ append_txn_prepared) == 1);

            total_size_ = RAFTLOG_HEADER_SIZE;

            total_size_ += 2 * RedoRecord::RedoRecord_HeadSize; // begin and prepared/commit log
            for (auto it = write_set_->begin(); it != write_set_->end(); ++it) {
                if(it -> getWType() == WType::INSERT_TUPLE){
                    total_size_ += RedoRecord::RedoRecord_HeadSize + sizeof(uint32_t) + it->getKeySize() + sizeof(uint32_t) + it->getValueSize();
                }
                else if(it -> getWType() == WType::DELETE_TUPLE){
                    total_size_ += RedoRecord::RedoRecord_HeadSize + sizeof(uint32_t) + it->getKeySize() + sizeof(uint32_t);
                }
            }
            redo_record_ = new char[total_size_-RAFTLOG_HEADER_SIZE];
            int ptr = 0;
            //txn begin
            RedoRecord record(RedoRecordType::BEGIN);
            memcpy(redo_record_ + ptr, &record, record.size_);
            ptr += record.size_;
            for (auto it = write_set_->begin(); it != write_set_->end(); ++it) {
                if(it->getWType() == WType::INSERT_TUPLE){
                    RedoRecord record(RedoRecordType::INSERT, it->getKeySize(), it->getKey().c_str(), it->getValueSize(), it->getValue().c_str());
                    memcpy(redo_record_ + ptr, &record, RedoRecord::RedoRecord_HeadSize);
                    ptr += RedoRecord::RedoRecord_HeadSize;
                    memcpy(redo_record_ + ptr, &record.key_size_, sizeof(uint32_t));
                    ptr += sizeof(uint32_t);
                    memcpy(redo_record_ + ptr, &record.key_, record.key_size_);
                    ptr += record.key_size_;

                    memcpy(redo_record_ + ptr, &record.value_size_, sizeof(uint32_t));
                    ptr += sizeof(uint32_t);
                    memcpy(redo_record_ + ptr, &record.value_, record.value_size_);
                    ptr += record.value_size_;
                }
                else{
                    RedoRecord record(RedoRecordType::DELETE, it->getKeySize(), it->getKey().c_str(), 0, nullptr);
                    memcpy(redo_record_ + ptr, &record, RedoRecord::RedoRecord_HeadSize);
                    ptr += RedoRecord::RedoRecord_HeadSize;
                    memcpy(redo_record_ + ptr, &record.key_size_, sizeof(uint32_t));
                    ptr += sizeof(uint32_t);
                    memcpy(redo_record_ + ptr, &record.key_, record.key_size_);
                    ptr += record.key_size_;

                }
            }
            //txn prepare/ txn commit
            if(append_txn_prepared){
                RedoRecord record(RedoRecordType::PREPARED);
                memcpy(redo_record_ + ptr, &record, record.size_);
                ptr += record.size_;
            }
            else if(append_txn_commit){
                RedoRecord record(RedoRecordType::COMMIT);
                memcpy(redo_record_ + ptr, &record, record.size_);
                ptr += record.size_;
            }
        }
    
    // use for txn commit and abort in 2PC
    RaftLogRecord(txn_id_t txn_id, RedoRecordType log_type)
        : txn_id_(txn_id) {
            assert(log_type == RedoRecordType::COMMIT || log_type == RedoRecordType::ABORT);
            total_size_ = RAFTLOG_HEADER_SIZE;
            total_size_ += RedoRecord::RedoRecord_HeadSize;
            redo_record_ = new char[total_size_-RAFTLOG_HEADER_SIZE];
            int ptr = 0;
            //txn begin
            RedoRecord record(log_type);
            memcpy(redo_record_ + ptr, &record, record.size_);
            ptr += record.size_;
        }

    inline int32_t GetSize() { return total_size_; }

    static std::vector<RedoRecord> Deserialize(const char* raftlog , txn_id_t *txn_id){ 
        // 解析RaftLogRecord, 并且重做操作
        int32_t tatal_size = *reinterpret_cast<const int32_t *>(raftlog);
        raftlog += sizeof(int32_t);
        *txn_id = *reinterpret_cast<const txn_id_t *>(raftlog);
        raftlog += sizeof(txn_id_t);

        int ptr = 0;
        std::vector<RedoRecord> res;
        while (ptr < tatal_size - RAFTLOG_HEADER_SIZE)
        {
            int32_t size = *reinterpret_cast<const int32_t *>(raftlog + ptr);
            ptr += sizeof(int32_t);
            RedoRecordType log_type = *reinterpret_cast<const RedoRecordType *>(raftlog + ptr);
            ptr += sizeof(RedoRecordType);
            assert(log_type != RedoRecordType::INVALID);
            switch (log_type) {
                case RedoRecordType::COMMIT:
                case RedoRecordType::ABORT:
                case RedoRecordType::BEGIN: 
                case RedoRecordType::PREPARED: {
                    res.push_back(RedoRecord(log_type));
                    break;
                }
                case RedoRecordType::INSERT: {
                    uint32_t key_size = *reinterpret_cast<const uint32_t *>(raftlog + ptr);
                    ptr += sizeof(uint32_t);
                    const char* key = raftlog + ptr;
                    ptr += key_size;

                    uint32_t value_size = *reinterpret_cast<const uint32_t *>(raftlog + ptr);
                    ptr += sizeof(uint32_t);
                    const char* value = raftlog + ptr;
                    ptr += value_size;

                    res.push_back(RedoRecord(log_type, key_size, key, value_size, value));
                    break;
                }
                case RedoRecordType::DELETE: {
                    uint32_t key_size = *reinterpret_cast<const uint32_t *>(raftlog + ptr);
                    ptr += sizeof(uint32_t);
                    const char* key = raftlog + ptr;
                    ptr += key_size;

                    res.push_back(RedoRecord(log_type, key_size, key, 0, nullptr));
                    break;
                }
                default:
                    std::cerr << "Invalid Log Type";
            }
        }
        return res;
    }

public:
    int32_t total_size_{0};
    txn_id_t txn_id_{INVALID_TXN_ID};
    //redo record
    char* redo_record_;
};

class RaftLogManager
{
private:
    TransactionManager* transaction_manager_;
    KVStore *kv_;

public:
    RaftLogManager(TransactionManager* transaction_manager, KVStore *kv):transaction_manager_(transaction_manager), kv_(kv){};
    ~RaftLogManager(){};

    // raft log为raft follower所接受到的的日志的字节流
    void RedoRaftLog(const char* raftlog){
        txn_id_t txn_id;
        std::vector<RedoRecord> res = RaftLogRecord::Deserialize(raftlog, &txn_id);
        Transaction* txn = nullptr;
        for(auto x: res){
            switch (x.log_type_)
            {
            case RedoRecordType::BEGIN :
                transaction_manager_->Begin(txn, txn_id); 
                break;
            case RedoRecordType::COMMIT:
                transaction_manager_->CommitSingle(txn);
                break;
            case RedoRecordType::ABORT:
                transaction_manager_->AbortSingle(txn);
                break; 
            case RedoRecordType::PREPARED:
                transaction_manager_->PrepareCommit(txn);
                break; 
            case RedoRecordType::INSERT:
                kv_->put(std::string(x.key_, x.key_size_), std::string(x.value_, x.value_size_), txn, true);
            case RedoRecordType::DELETE:
                kv_->del(std::string(x.key_, x.key_size_), txn, true);
            case RedoRecordType::CREATE_TABLE:
            case RedoRecordType::DROP_TABLE:
                // Hcy: todo
            default:
                break;
            }
        }
        
    }
};

