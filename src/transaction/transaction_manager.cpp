#pragma once

#include "Transaction.h"
#include "Lock_manager.h"

enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0, BASIC_TO };

class TransactionManager
{
private:
    /* data */
public:
    TransactionManager(){};
    ~TransactionManager(){};

    explicit TransactionManager(Lock_manager *lock_manager, 
            ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING) {
        
    }
};