#pragma once

#include "Transaction.h"
#include "Lock_manager.h"
#include "meta_service.pb.h"

#include <shared_mutex>
#include <brpc/channel.h>
#include <butil/logging.h>

#define server "[::1]:8001"

enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0 };

class TransactionManager
{
private:
    Lock_manager *lock_manager_;
    LogManager *log_manager_;
    ConcurrencyMode concurrency_mode_;

    /** The global transaction latch is used for checkpointing. */
    std::shared_mutex global_txn_latch_;

    void ReleaseLocks(Transaction *txn);

public:
    ~TransactionManager() = default;
    explicit TransactionManager(Lock_manager *lock_manager, LogManager *log_manager = nullptr,
            ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING) {
        lock_manager_ = lock_manager;
        log_manager_ = log_manager;
        concurrency_mode_ = concurrency_mode;
    }

    ConcurrencyMode getConcurrencyMode() { return concurrency_mode_; }
    void SetConcurrencyMode(ConcurrencyMode concurrency_mode) { concurrency_mode_ = concurrency_mode; }
    Lock_manager *getLockManager() { return lock_manager_; }

    /* The transaction map is a local list of all the running transactions in the local node. */
    static std::unordered_map<txn_id_t, Transaction *> txn_map;
    static std::shared_mutex txn_map_mutex;

    static Transaction *getTransaction(txn_id_t txn_id) {
        std::shared_lock<std::shared_mutex> l(txn_map_mutex);
        if(TransactionManager::txn_map.find(txn_id) == TransactionManager::txn_map.end()){
            return nullptr;
        }
        Transaction *txn = txn_map[txn_id];
        assert(txn != nullptr);
        return txn;
    }

    uint64_t getTimestampFromServer();

    Transaction* Begin(Transaction* txn, IsolationLevel isolation_level );

    void Abort(Transaction * txn);

    void Commit(Transaction * txn);

    /** Prevents all transactions from performing operations, used for checkpointing. */
    void BlockAllTransactions(){global_txn_latch_.lock();}

    /** Resumes all transactions, used for checkpointing. */
    void ResumeTransactions(){global_txn_latch_.unlock();}
    
};