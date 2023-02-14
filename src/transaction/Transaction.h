#pragma once

#include <cstdint>

using txn_id_t = int32_t;

class Transaction
{
private:
    txn_id_t txn_id;
    
public:
    Transaction(/* args */);
    ~Transaction();
};

Transaction::Transaction(/* args */)
{

}

Transaction::~Transaction()
{
}
