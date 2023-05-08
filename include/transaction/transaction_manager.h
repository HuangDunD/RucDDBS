#pragma once

#include "Transaction.h"
#include "Lock_manager.h"
#include "meta_service.pb.h"
// #include "storage/KVStore.h"
// #include "Inmemory/KVStore.h"
// #include "storage/KVStore.h"
#include "Inmemory/KVStore_new.h"

#include <shared_mutex>

enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0 };

class TransactionManager
{
private:
    Lock_manager *lock_manager_;
    LogManager *log_manager_;
    KVStore *kv_;
    ConcurrencyMode concurrency_mode_;

    /** The global transaction latch is used for checkpointing. */
    std::shared_mutex global_txn_latch_;

    void ReleaseLocks(Transaction *txn);

public: 
    ~TransactionManager() = default;
    explicit TransactionManager(Lock_manager *lock_manager, KVStore *kv, LogManager *log_manager = nullptr, 
            ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING) {
        lock_manager_ = lock_manager;
        log_manager_ = log_manager;
        kv_ = kv;
        concurrency_mode_ = concurrency_mode;
    }

    ConcurrencyMode getConcurrencyMode() { return concurrency_mode_; }
    void SetConcurrencyMode(ConcurrencyMode concurrency_mode) { concurrency_mode_ = concurrency_mode; }
    Lock_manager *getLockManager() { return lock_manager_; }
    KVStore* getKVstore() {return kv_; }
    
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

    // 从meta_server获取时间戳作为txn id
    Transaction* Begin(Transaction*& txn, IsolationLevel isolation_level=IsolationLevel::SERIALIZABLE);

    // 传入txn_id作为事务id
    Transaction* Begin(Transaction*& txn, txn_id_t txn_id, IsolationLevel isolation_level=IsolationLevel::SERIALIZABLE); 
    
    /// @brief 回滚事务（需要判断是分布式事务还是单机事务）如果是分布式事务，
    /// 需要作为rpc的客户端向各个参与者发送回滚请求
    /// @param txn 事务指针
    /// @return 回滚成功返回true，失败返回false
    bool Abort(Transaction * txn);
    /// @brief 本地执行事务回滚
    bool AbortSingle(Transaction * txn);
    /// @brief 协调者收到事务提交请求的函数入口，如果是分布式事务，发起两阶段提交，
    /// 向各个参与者发送准备提交请求并得到事务结果，之后向各个参与者发送事务结果
    bool Commit(Transaction * txn);
    /// @brief 本地执行事务提交
    bool CommitSingle(Transaction * txn);
    /// @brief 准备提交函数，写入相关日志并返回准备结果
    bool PrepareCommit(Transaction * txn);

    /** Prevents all transactions from performing operations, used for checkpointing. */
    void BlockAllTransactions(){global_txn_latch_.lock();}

    /** Resumes all transactions, used for checkpointing. */
    void ResumeTransactions(){global_txn_latch_.unlock();}
    
};