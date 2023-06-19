#include "benchmark_txn.h"

#include <butil/logging.h> 
#include <brpc/channel.h>
#include <random>
#include <future>

Transaction* Benchmark_Txn::Generate(double read_ratio){
    Transaction* txn = nullptr;
    transaction_manager_->Begin(txn);

    // 设置随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis_txn_node_cnt(2, node_cnt);
    std::uniform_int_distribution<int> dis_index(0, node_cnt-1);
    std::uniform_real_distribution<double> dis_read_op(0.0, 1.0);
    std::uniform_int_distribution<int> dis_op_num(5, FLAGS_OP_MAX_NUM);

    uint32_t txn_node_cnt = dis_txn_node_cnt(gen); // 事务节点数量

    std::vector<std::vector<BenchMark_Operator>> ops(txn_node_cnt); //存放各个节点的操作
    std::vector<int> op_partition_num(txn_node_cnt);

    uint32_t operator_num = dis_op_num(gen);

    std::unordered_set<int> index_set;

    if(txn_node_cnt > 1){ 
        txn->set_is_distributed(true);
    }
    
    if(txn_node_cnt==1){
        op_partition_num[0] = operator_num;
    }
    else if(txn_node_cnt==2){
        int theta = (1-FLAGS_OP_Theta);
        op_partition_num[0] = 0.5 * operator_num * theta + 1;
        op_partition_num[1] = operator_num - op_partition_num[0];
    }
    else if(txn_node_cnt==3){
        int theta = (1-FLAGS_OP_Theta);
        op_partition_num[0] = operator_num / 3 * theta + 1;
        op_partition_num[1] = operator_num / 3;
        op_partition_num[2] = operator_num - op_partition_num[0] - op_partition_num[1];
    }

    while(index_set.size()<txn_node_cnt){ 
        int index = dis_index(gen);
        if(index_set.count(index)) 
            continue;
        index_set.emplace(index);

        IP_Port ip = NodeSet[index];
        txn->get_distributed_node_set()->push_back(ip);

        int cur_gen_size = 0;

        while(cur_gen_size++ < op_partition_num[index_set.size()-1]){
            if(dis_read_op(gen) <= read_ratio){
                //生成读操作
                BenchMark_Operator op;
                op.txn_id = txn->get_txn_id();
                op.key = "key" + std::to_string(rand() % FLAGS_BANCHMARK_NUM);
                op.op_type = BenchMark_Operator::OP_TYPE::Get;
                // op.node_id = txn->get_distributed_node_set()->size()-1;
                // op.node_id = index;
                ops[index_set.size()-1].push_back(op);
            }
            else{
                //生成写操作
                BenchMark_Operator op;
                op.txn_id = txn->get_txn_id();
                op.key = "key" + std::to_string(rand() % FLAGS_BANCHMARK_NUM);
                op.value = "value" + std::to_string(rand() % FLAGS_BANCHMARK_NUM);
                op.op_type = rand()%2 ? (BenchMark_Operator::OP_TYPE::Put) : (BenchMark_Operator::OP_TYPE::Del);
                // op.node_id = txn->get_distributed_node_set()->size()-1;
                ops[index_set.size()-1].push_back(op);
            }
        }
    }// 生成分布式事务ip and port

    brpc::ChannelOptions options;
    // options.connection_type = "pooled";
    options.timeout_ms = 10000;
    options.max_retry = 3;

    // 获取当前时间点
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<bool>> futures_start;
    for(auto x: index_set){
        futures_start.push_back(std::async(std::launch::async, [x, &options, &txn, this](){
            brpc::Channel channel;
            IP_Port ip = NodeSet[x];
            if (channel.Init(ip.ip_addr.c_str(), ip.port, &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
            }
            benchmark_service::BenchmarkService_Stub stub_start(&channel);
            benchmark_service::StartTxnRequest request;
            benchmark_service::StartTxnResponse response;
            request.set_txn_id(txn->get_txn_id());
            brpc::Controller cntl;
            stub_start.StartTxn(&cntl, &request, &response, NULL);
            if (cntl.Failed()) {
                LOG(WARNING) << cntl.ErrorText();
                transaction_manager_->Abort(txn);
                return false;
            }
            cntl.Reset();
            return true;
        }));
    }
    for(size_t i=0; i<(*txn->get_distributed_node_set()).size(); i++){
        if(futures_start[i].get() == false){
            return txn;
        }
    }

    auto start_finish = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;
    int ii = 0;
    for(auto node: *txn->get_distributed_node_set()){
        futures.push_back(std::async(std::launch::async, [&txn, node, &options, ii, &ops](){
            brpc::Channel channel;
            IP_Port ip = txn->get_distributed_node_set()->at(ii);
            if (channel.Init(ip.ip_addr.c_str(), ip.port, &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
            }
            benchmark_service::BenchmarkService_Stub stub_oper(&channel);
            benchmark_service::OperatorRequest request;
            benchmark_service::OperatorResponse response;
            request.set_txn_id(txn->get_txn_id());
            for(auto x: ops[ii]){
                benchmark_service::Operator *op = new benchmark_service::Operator();
                op->set_key(x.key);
                op->set_value(x.value);
                switch (x.op_type)
                {
                case BenchMark_Operator::OP_TYPE::Get :
                    op->set_op_type(benchmark_service::Operator_OP_TYPE::Operator_OP_TYPE_Get);
                    break;
                case BenchMark_Operator::OP_TYPE::Put :
                    op->set_op_type(benchmark_service::Operator_OP_TYPE::Operator_OP_TYPE_Put);
                    break;   
                case BenchMark_Operator::OP_TYPE::Del :
                    op->set_op_type(benchmark_service::Operator_OP_TYPE::Operator_OP_TYPE_Del);
                    break;   
                default:
                    break;
                }
                request.mutable_ops()->AddAllocated(op);

            }
            brpc::Controller cntl;
            stub_oper.SendOperator(&cntl, &request, &response, NULL);
            if (cntl.Failed()) {
                LOG(WARNING) << cntl.ErrorText();
            }
            request.mutable_ops()->Clear();
            cntl.Reset();
        }));
        ii++;
    }
    for(size_t i=0; i<(*txn->get_distributed_node_set()).size(); i++){
        futures[i].get();
    }
    
    // 获取当前时间点
    auto exec_finish = std::chrono::high_resolution_clock::now();
    // int cur_node_id = -1;
    // brpc::Channel channel;
    // for(auto x: ops){
    //     if( x.node_id != cur_node_id){
    //         IP_Port ip = txn->get_distributed_node_set()->at(x.node_id);
    //         if (channel.Init(ip.ip_addr.c_str(), ip.port, &options) != 0) {
    //             LOG(ERROR) << "Fail to initialize channel";
    //         }
    //         cur_node_id = x.node_id;
    //     }
        
    //     benchmark_service::BenchmarkService_Stub stub_oper(&channel);
    //     benchmark_service::OperatorRequest request;
    //     benchmark_service::OperatorResponse response;
        
    //     request.set_txn_id(x.txn_id);
    //     request.set_key(x.key);
    //     request.set_value(x.value);
    //     switch (x.op_type)
    //     {
    //     case BenchMark_Operator::OP_TYPE::Get :
    //         request.set_op_type(benchmark_service::OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Get);
    //         break;
    //     case BenchMark_Operator::OP_TYPE::Put :
    //         request.set_op_type(benchmark_service::OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Put);
    //         break;   
    //     case BenchMark_Operator::OP_TYPE::Del :
    //         request.set_op_type(benchmark_service::OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Del);
    //         break;   
    //     default:
    //         break;
    //     }

    //     brpc::Controller cntl;
    //     stub_oper.SendOperator(&cntl, &request, &response, NULL);
    //     if (cntl.Failed()) {
    //         LOG(WARNING) << cntl.ErrorText();
    //         break;
    //     }
    //     if(response.ok() == false) break;
    //     cntl.Reset();
    // }

    if(transaction_manager_->Commit(txn))
        commit_txn_cnt_++;
    else
        abort_txn_cnt_++;

    // 获取当前时间点
    auto end = std::chrono::high_resolution_clock::now();
    // 计算代码执行时间
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // 计算开始时间
    auto start_duration = std::chrono::duration_cast<std::chrono::milliseconds>(start_finish - start);
    // 计算执行时间
    auto exec_duration = std::chrono::duration_cast<std::chrono::milliseconds>(exec_finish - start_finish);
    // 计算提交时间
    auto commit_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - exec_finish);

    latency_ms_ += duration.count();
    exec_ms_ += exec_duration.count();
    commit_ms_ += commit_duration.count();
    start_ms_ += start_duration.count();
    return txn;
}