#pragma once

#include <mutex>
#include <Transaction.h>

static const std::string GroupLockModeStr[10] = {"NON_LOCK", "IS", "IX", "S", "X", "SIX"};

class Lock_manager
{
public:
    enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX };
    
    /**
     * @brief the locks on data
     * NON_LOCK: there is no lock on the data
     * IS : intention shared lock
     * IX : intention exclusive lock
     * S : shared lock
     * X : exclusive lock
     * SIX : shared lock + intention exclusive lock(locks are from the same transaction)
     */
    enum class GroupLockMode { NON_LOCK, IS, IX, S, X, SIX};

    class LockRequest {
    public:
        LockRequest(txn_id_t txn_id, LockMode lock_mode)
            : txn_id_(txn_id), lock_mode_(lock_mode)/*, granted_(false)*/ {}

        txn_id_t txn_id_;
        LockMode lock_mode_;
        // bool granted_;
    };

public:
    Lock_manager(/* args */);

    ~Lock_manager();

    bool LockSharedOnRecord(Transaction *txn, const Rid &rid, int tab_fd);




private:
    std::mutex latch_;  // 互斥锁，用于锁表的互斥访问
};

Lock_manager::Lock_manager(/* args */)
{
    
}

Lock_manager::~Lock_manager()
{

}
