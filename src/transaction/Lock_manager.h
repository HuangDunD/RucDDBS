#pragma once

#include "txn.h"
#include "Transaction.h"
#include <mutex>
#include <condition_variable>  // NOLINT
#include <list>
#include <unordered_map>

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
        /** List of lock requests for the same resource (table, partition or row) */
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

    auto LockPartition(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id) -> bool;

    auto UnLockPartition(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id) -> bool;

    auto LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool;

    auto UnLockRow(Transaction *txn,  const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool;

    auto Unlock(Transaction *txn, const Lock_data_id &l_id ) -> bool;

    auto isLockCompatible(const LockRequest *lock_request, const LockMode &target_lock_mode) -> bool;

    auto isUpdateCompatible(const LockRequest *lock_request, const LockMode &upgrade_lock_mode) -> bool; 

    static auto checkQueueCompatible(const LockRequestQueue *request_queue, const LockRequest *request) -> bool;

private:
    std::mutex latch_;  // 锁表的互斥锁，用于锁表的互斥访问
    std::unordered_map<Lock_data_id, LockRequestQueue> lock_map_;  //可上锁的数据(表,分区,行)数据与锁请求队列的对应关系

};
