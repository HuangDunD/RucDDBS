#pragma once

#include <mutex>
#include "Transaction.h"
#include <condition_variable>  // NOLINT
#include <list>
#include <cassert>

using table_oid_t = int32_t;
using row_id_t = int32_t;
using partition_id_t = int32_t;

enum class Lock_data_type {TABLE, PARTITION, ROW};

class Lock_data_id
{
public:
    Lock_data_type type_;
    table_oid_t oid_;
    partition_id_t p_id_;
    row_id_t row_id_;

    bool operator==(const Lock_data_id &other) const{
        if(type_ != other.type_) return false;
        if(oid_ != other.oid_) return false;
        if(p_id_ != other.p_id_) return false;
        if(row_id_ != other.row_id_) return false;
        return true;
    }

    // 构造64位大小数据，用于hash，可减少冲突的可能
    // bit structure
    // |    **   |    **********    |  ****************  |   *******************************   |
    // |2bit type|   14bit table    |   16bit partition  |               32 bit rowid          |
    size_t Get() const{
        return static_cast<size_t> (type_) << 62 | static_cast<size_t> (oid_) << 47 | 
            static_cast<size_t> (p_id_) << 32 | static_cast<size_t> (row_id_);
    }

public:
    Lock_data_id(table_oid_t oid, Lock_data_type type):
        oid_(oid), type_(type){
        assert(type_ == Lock_data_type::TABLE);
    };
    Lock_data_id(table_oid_t oid, partition_id_t p_id, Lock_data_type type):
        oid_(oid), p_id_(p_id), type_(type){
        assert(type_ == Lock_data_type::PARTITION);
    };
    Lock_data_id(table_oid_t oid, partition_id_t p_id, row_id_t row_id, Lock_data_type type):
        oid_(oid), p_id_(p_id), row_id_(row_id), type_(type){
        assert(type_ == Lock_data_type::ROW);
    };
    ~Lock_data_id(){};
};

template<>
struct std::hash<Lock_data_id>
{
    size_t operator() (const Lock_data_id& lock_data) const noexcept
    {
        return hash<size_t>()(lock_data.Get());
    }
}; // 间接调用原生Hash.


class Lock_manager
{
public:
    enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX };

    class LockRequest {
    public:
        LockRequest(txn_id_t txn_id, LockMode lock_mode)
            : txn_id_(txn_id), lock_mode_(lock_mode){}

        txn_id_t txn_id_;
        LockMode lock_mode_;
        bool granted_ = false;
    };

    class LockRequestQueue {
    public:
        /** List of lock requests for the same resource (table or row) */
        std::list<LockRequest *> request_queue_;
        /** For notifying blocked transactions on this rid */
        std::condition_variable cv_;
        // waiting bit: if there is a lock request which is not granted, the waiting -bit will be true
        bool is_waiting_ = false;
        // upgrading_flag: if there is a lock waiting for upgrading, other transactions that request for upgrading will be aborted
        // (deadlock prevetion)
        bool upgrading_ = false;                    // 当前队列中是否存在一个正在upgrade的锁
        /** coordination */
        std::mutex latch_;
        // the number of shared locks
        int shared_lock_num_ = 0;
        // the number of IX locks
        int IX_lock_num_ = 0;
    };


public:
    Lock_manager(){};

    ~Lock_manager(){};

    auto LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool;

    auto UnLockTable(Transaction *txn, const table_oid_t &oid) -> bool;

    auto LockPartition(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id) -> bool;

    auto UnLockPartition(Transaction *txn, const table_oid_t &oid) -> bool;

    auto LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const row_id_t &row_id) -> bool;

    auto UnLockRow(Transaction *txn,  const table_oid_t &oid, const row_id_t &row_id) -> bool;


private:
    std::mutex table_lock_latch_;  // 表锁表的互斥锁，用于表锁表的互斥访问
    std::mutex partition_lock_latch_;  // 分区锁表的互斥锁，用于表锁表的互斥访问
    std::mutex row_lock_latch_;  // 行锁表的互斥锁，用于表锁表的互斥访问
};
