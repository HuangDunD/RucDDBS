#pragma once

#include <cstdint>

enum class TransactionState { DEFAULT, GROWING, SHRINKING, COMMITTED, ABORTED };
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED, SERIALIZABLE };

using txn_id_t = int32_t;

class Transaction
{
private:
    txn_id_t txn_id;
    TransactionState state_;
    IsolationLevel isolation_ ;

public:
    inline TransactionState get_state() const {return state_;}

    inline void set_transaction_state(TransactionState state){
        state_ = state;
    }

    inline IsolationLevel get_isolation() const {return isolation_;}

    Transaction(){};
    ~Transaction(){};
};

