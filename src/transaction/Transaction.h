#pragma once

#include "txn.h"
#include <cstdint>
#include <unordered_set>
#include <memory>
#include <utility> 
#include <thread>

enum class TransactionState { DEFAULT, GROWING, SHRINKING, COMMITTED, ABORTED };
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED, SERIALIZABLE };

using txn_id_t = int32_t;

/**
 * Reason to a transaction abortion
 */
enum class AbortReason {
  LOCK_ON_SHRINKING,
  UPGRADE_CONFLICT,
  LOCK_SHARED_ON_READ_UNCOMMITTED,
  TABLE_LOCK_NOT_PRESENT,
  PARTITION_LOCK_NOT_PRESENT, 
  ATTEMPTED_INTENTION_LOCK_ON_ROW,
  TABLE_UNLOCKED_BEFORE_UNLOCKING_ROWS,
  INCOMPATIBLE_UPGRADE,
  ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD, 
  DEAD_LOCK_PREVENT_NO_WAIT
};

/**
 * TransactionAbortException is thrown when state of a transaction is changed to ABORTED
 */
class TransactionAbortException : public std::exception {
  txn_id_t txn_id_;
  AbortReason abort_reason_;

 public:
  explicit TransactionAbortException(txn_id_t txn_id, AbortReason abort_reason)
      : txn_id_(txn_id), abort_reason_(abort_reason){}
  auto GetTransactionId() -> txn_id_t { return txn_id_; }
  auto GetAbortReason() -> AbortReason { return abort_reason_; }
  auto GetInfo() -> std::string {
    switch (abort_reason_) {
      case AbortReason::LOCK_ON_SHRINKING:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because it can not take locks in the shrinking state";
      case AbortReason::UPGRADE_CONFLICT:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because another transaction is already waiting to upgrade its lock";
      case AbortReason::LOCK_SHARED_ON_READ_UNCOMMITTED:
        return "Transaction " + std::to_string(txn_id_) + " aborted on lockshared on READ_UNCOMMITTED";
      case AbortReason::TABLE_LOCK_NOT_PRESENT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because table lock not present";
      case AbortReason::PARTITION_LOCK_NOT_PRESENT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because table lock not present";
      case AbortReason::ATTEMPTED_INTENTION_LOCK_ON_ROW:
        return "Transaction " + std::to_string(txn_id_) + " aborted because intention lock attempted on row";
      case AbortReason::TABLE_UNLOCKED_BEFORE_UNLOCKING_ROWS:
        return "Transaction " + std::to_string(txn_id_) +
               " aborted because table locks dropped before dropping row locks";
      case AbortReason::INCOMPATIBLE_UPGRADE:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted lock upgrade is incompatible";
      case AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted to unlock but no lock held ";
      case AbortReason::DEAD_LOCK_PREVENT_NO_WAIT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted to lock but need to wait(deadlock prevent) ";
    }
    // Todo: Should fail with unreachable.
    return "";
  }
};

class Transaction
{
private:
    txn_id_t txn_id_;
    TransactionState state_;
    IsolationLevel isolation_ ;
    std::thread::id thread_id_;

    // std::shared_ptr<std::unordered_set<std::pair<Lock_data_id, LockMode>>> all_lock_set_;

    std::shared_ptr<std::unordered_set<Lock_data_id>> table_S_lock_set_;
    std::shared_ptr<std::unordered_set<Lock_data_id>> table_X_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> table_IS_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> table_IX_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> table_SIX_lock_set_;

    std::shared_ptr<std::unordered_set<Lock_data_id>> partition_S_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> partition_X_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> partition_IS_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> partition_IX_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> partition_SIX_lock_set_;  

    std::shared_ptr<std::unordered_set<Lock_data_id>> row_S_lock_set_;  
    std::shared_ptr<std::unordered_set<Lock_data_id>> row_X_lock_set_;  


public:

    inline txn_id_t get_txn_id() const { return txn_id_; }

    inline TransactionState get_state() const {return state_;}

    inline void set_transaction_state(TransactionState state){
        state_ = state;
    }

    inline IsolationLevel get_isolation() const {return isolation_;}

    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_table_S_lock_set() {return table_S_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_table_X_lock_set() {return table_X_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_table_IS_lock_set() {return table_IS_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_table_IX_lock_set() {return table_IX_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_table_SIX_lock_set() {return table_SIX_lock_set_;} 

    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_partition_S_lock_set() {return partition_S_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_partition_X_lock_set() {return partition_X_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_partition_IS_lock_set() {return partition_IS_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_partition_IX_lock_set() {return partition_IX_lock_set_;} 
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_partition_SIX_lock_set() {return partition_SIX_lock_set_;}

    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_row_S_lock_set() {return row_S_lock_set_;}
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_row_X_lock_set() {return row_X_lock_set_;}

    inline void add_lock_set(LockMode lock_mode, Lock_data_id l_id){
      switch (l_id.type_)
      {
        case Lock_data_type::TABLE:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            table_S_lock_set_->emplace(l_id);
            break;
          case LockMode::EXLUCSIVE:
            table_X_lock_set_->emplace(l_id);
            break;
          case LockMode::INTENTION_SHARED:
            table_IS_lock_set_->emplace(l_id);
            break;
          case LockMode::INTENTION_EXCLUSIVE:
            table_IX_lock_set_->emplace(l_id);
            break;
          case LockMode::S_IX:
            table_SIX_lock_set_->emplace(l_id);
            break;
          default:
            break;
          }
        break;
          case Lock_data_type::PARTITION:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            partition_S_lock_set_->emplace(l_id);
            break;
          case LockMode::EXLUCSIVE:
            partition_X_lock_set_->emplace(l_id);
            break;
          case LockMode::INTENTION_SHARED:
            partition_IS_lock_set_->emplace(l_id);
            break;
          case LockMode::INTENTION_EXCLUSIVE:
            partition_IX_lock_set_->emplace(l_id);
            break;
          case LockMode::S_IX:
            partition_SIX_lock_set_->emplace(l_id);
            break;
          default:
            break;
          }
        break;
          case Lock_data_type::ROW:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            row_S_lock_set_->emplace(l_id);
            break;
          case LockMode::EXLUCSIVE:
            row_X_lock_set_->emplace(l_id);
            break;
          default:
            break;
          }
        break;
      }
    }

    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_lock_set(Lock_data_type type, LockMode lock_mode)
    {
      switch (type)
      {
        case Lock_data_type::TABLE:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            return table_S_lock_set_;
          case LockMode::EXLUCSIVE:
            return table_X_lock_set_;
          case LockMode::INTENTION_SHARED:
            return table_IS_lock_set_;
          case LockMode::INTENTION_EXCLUSIVE:
            return table_IX_lock_set_;
          case LockMode::S_IX:
            return table_SIX_lock_set_;
          default:
            break;
          }
        break;
          case Lock_data_type::PARTITION:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            return partition_S_lock_set_;
          case LockMode::EXLUCSIVE:
            return partition_X_lock_set_;
          case LockMode::INTENTION_SHARED:
            return partition_IS_lock_set_;
          case LockMode::INTENTION_EXCLUSIVE:
            return partition_IX_lock_set_;
          case LockMode::S_IX:
            return partition_SIX_lock_set_;
          default:
            break;
          }
        break;
          case Lock_data_type::ROW:
          switch (lock_mode)
          {
          case LockMode::SHARED:
            return row_S_lock_set_;
          case LockMode::EXLUCSIVE:
            return row_X_lock_set_;
          default:
            break;
          }
        break;
        default:
        break;
      }
      return nullptr;
    }

    // inline std::shared_ptr<std::unordered_set<std::pair<Lock_data_id, LockMode>>> get_all_lock_set_() {return all_lock_set_;}

    explicit Transaction(txn_id_t txn_id, IsolationLevel isolation_level = IsolationLevel::SERIALIZABLE)
      : txn_id_(txn_id), state_(TransactionState::DEFAULT), isolation_(isolation_level)
    {
        table_S_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        table_X_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        table_IS_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        table_IX_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        table_SIX_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();

        partition_S_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        partition_X_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        partition_IS_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        partition_IX_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        partition_SIX_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();

        row_S_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();
        row_X_lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();

        thread_id_ = std::this_thread::get_id();
    };

    ~Transaction(){};
};

