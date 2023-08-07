#pragma once

#include "txn.h"
#include "recovery/log_record.h"
#include "recovery/log_manager.h"
#include <cstdint>
#include <unordered_set>
#include <memory>
#include <utility> 
#include <thread>
#include <deque>
#include <vector>

enum class TransactionState { DEFAULT, GROWING, SHRINKING, COMMITTED, ABORTED, PREPARED };
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED, SERIALIZABLE };

struct IP_Port{
    std::string ip_addr;
    uint32_t port;
    bool operator==(const IP_Port& other) const {
        return ip_addr == other.ip_addr && port == other.port;
    }
};

struct IP_PortHash {
    std::size_t operator()(const IP_Port& obj) const {
        // 哈希函数的实现
        std::size_t h1 = std::hash<std::string>{}(obj.ip_addr);
        std::size_t h2 = std::hash<int>{}(obj.port);
        return h1 ^ h2;
    }
};

using txn_id_t = uint64_t;

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
  ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD, 
  DEAD_LOCK_PREVENT_NO_WAIT,
  PARTICIPANT_RETURN_ABORT
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
      case AbortReason::ATTEMPTED_UNLOCK_BUT_NO_LOCK_HELD:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted to unlock but no lock held ";
      case AbortReason::DEAD_LOCK_PREVENT_NO_WAIT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because attempted to lock but need to wait(deadlock prevent) ";
      case AbortReason::PARTICIPANT_RETURN_ABORT:
        return "Transaction " + std::to_string(txn_id_) + " aborted because its participants return abort";
        return "";
    }
    return "";
  }
};

class Transaction
{
private:
    txn_id_t txn_id_;  //从metaserver中获取的全局唯一严格递增的混合逻辑时间戳
    TransactionState state_; 
    IsolationLevel isolation_ ;
    std::thread::id thread_id_;

    lsn_t prev_lsn_; //// 当前事务执行的最后一条操作对应的lsn
    std::shared_ptr<std::deque<WriteRecord>> write_set_;  // 事务包含的所有写操作

    bool is_distributed; //是否是分布式事务
    std::shared_ptr<std::unordered_set<IP_Port, IP_PortHash>> distributed_plan_excution_node_; //分布式执行计划涉及节点
    IP_Port coor_ip_;// 协调者节点

    std::shared_ptr<std::unordered_set<Lock_data_id>> lock_set_;  // 事务申请的所有锁

public:

    inline txn_id_t get_txn_id() const { return txn_id_; }

    inline TransactionState get_state() const {return state_;}

    inline void set_transaction_state(TransactionState state){ state_ = state; }

    inline IsolationLevel get_isolation() const {return isolation_;}

    inline lsn_t get_prev_lsn() const {return prev_lsn_; }
    inline void set_prev_lsn(lsn_t prev_lsn) { prev_lsn_ = prev_lsn; }

    inline std::shared_ptr<std::deque<WriteRecord>> get_write_set() {return write_set_;}

    inline bool get_is_distributed() const { return is_distributed; } //如果查询只涉及一个节点, 那么is_distributed=false, 否则为true
    inline void set_is_distributed(bool val) {is_distributed = val; } 
    inline std::shared_ptr<std::unordered_set<IP_Port, IP_PortHash>> get_distributed_node_set() {return distributed_plan_excution_node_;}
    inline void add_distributed_node_set(std::string ip_port) { 
      auto n = ip_port.rfind(':');
      if (n != std::string::npos) {
          std::string ip = ip_port.substr(0, n);
          uint32_t port = std::stoi(ip_port.substr(n+1, ip_port.length()-n-1));
          IP_Port ip_port = {ip, port};
          distributed_plan_excution_node_->emplace(ip_port);
          if( distributed_plan_excution_node_->size() > 1 ){
            is_distributed = true;
          }
      } else {
          // 处理找不到 '.' 字符的情况
          std::cout << "Invalid ip and ip " << std::endl;
      }
    }
    inline IP_Port get_coor_ip() {return coor_ip_; }
    inline void set_coor_ip(IP_Port ip) { coor_ip_ = ip; }
    inline std::shared_ptr<std::unordered_set<Lock_data_id>> get_lock_set() { return lock_set_; }

    explicit Transaction(txn_id_t txn_id, IsolationLevel isolation_level = IsolationLevel::SERIALIZABLE)
      : txn_id_(txn_id), state_(TransactionState::DEFAULT), isolation_(isolation_level)
    {
        distributed_plan_excution_node_ = std::make_shared<std::unordered_set<IP_Port, IP_PortHash>>(); 
        write_set_ = std::make_shared<std::deque<WriteRecord>>();
        lock_set_ = std::make_shared<std::unordered_set<Lock_data_id>>();

        is_distributed = false;
        prev_lsn_ = INVALID_LSN;
        thread_id_ = std::this_thread::get_id();   
    };

    ~Transaction(){};
};

