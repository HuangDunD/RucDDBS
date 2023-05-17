#pragma once
#include "transaction_manager_rpc.h"
#include "transaction_manager.h"
#include "benchmark_rpc.h"
#include "benchmark_config.h"

#include "gtest/gtest.h"
#include "benchmark_config.h"

class BenchMark_Operator{
public:
enum class OP_TYPE{ Get, Put, Del };
public:
    txn_id_t txn_id;    
    std::string key;
    std::string value;
    uint16_t node_id; 
    OP_TYPE op_type; 
};

class Benchmark_Txn
{
private:
    TransactionManager* transaction_manager_;
    int node_cnt;
public:
    explicit Benchmark_Txn(TransactionManager* transaction_manager)
        :transaction_manager_(transaction_manager){
        node_cnt = NodeSet.size();
    };
    ~Benchmark_Txn(){};
    Transaction* Generate(int read_ratio);
};