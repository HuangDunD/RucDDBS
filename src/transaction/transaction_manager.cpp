#include <brpc/channel.h>
#include <butil/logging.h>
#include <future>
#include <functional>

#include "transaction_manager.h"
#include "transaction_manager.pb.h"
#include "dbconfig.h"
#include "raft_log_record.h"
#include "raft_log.pb.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};
std::shared_mutex TransactionManager::txn_map_mutex = {};

uint64_t TransactionManager::getTimestampFromServer(){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 10000;
    options.max_retry = 3;

    if (channel.Init(FLAGS_META_SERVER_ADDR.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }
    meta_service::MetaService_Stub stub(&channel);
    brpc::Controller cntl;
    meta_service::getTimeStampRequest request;
    meta_service::getTimeStampResponse response;
    stub.GetTimeStamp(&cntl, &request, &response, NULL);

    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    return response.timestamp();
}

void TransactionManager::ReleaseLocks(Transaction *txn){
    for(auto iter = txn->get_lock_set()->begin(); iter != txn->get_lock_set()->end(); ++iter){
        lock_manager_->Unlock(txn, *iter);
    }
    return ;
}

Transaction* TransactionManager::Begin(Transaction*& txn, txn_id_t txn_id, IsolationLevel isolation_level){
    global_txn_latch_.lock_shared();

    if (txn == nullptr) {
        txn = new Transaction(txn_id, isolation_level); 
    }

    if (enable_logging) {
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::BEGIN);
        lsn_t lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
    }

    // std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    if(txn_map.find(txn->get_txn_id()) != txn_map.end()) 
        std::cout <<"******************************" << std::endl;
    txn_map[txn->get_txn_id()] = txn;
    return txn;
}

Transaction* TransactionManager::Begin(Transaction*& txn, IsolationLevel isolation_level){

    // 1. 判断传入事务参数是否为空指针
    // 2. 如果为空指针，创建新事务
    // 3. 把开始事务加入到全局事务表中
    // 4. 返回当前事务指针
    global_txn_latch_.lock_shared();

    if (txn == nullptr) {
        txn = new Transaction(getTimestampFromServer(), isolation_level); 
    }

    if (enable_logging) {
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::BEGIN);
        lsn_t lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
    }

    std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    txn_map[txn->get_txn_id()] = txn;
    return txn;
}

bool TransactionManager::AbortSingle(Transaction * txn, bool use_raft){
    // std::cout << txn->get_txn_id() << " " << "abort" << std::endl;
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        auto &item = write_set->back();
        if(item.getWType() == WType::DELETE_TUPLE){
            kv_->put(item.getKey(), item.getValue(), txn, false);
        }else if(item.getWType() == WType::INSERT_TUPLE){
            kv_->del(item.getKey(), txn, false);
        }
        // else if (item.getWType() == WType::UPDATE_TUPLE){
        //     kv_->del(std::string(item.getKey(),item.getKeySize()), txn);
        //     kv_->put(std::string(item.getKey(), item.getKeySize()), std::string(item.getOldValue(), item.getOldValueSize()), txn);
        // }
        write_set->pop_back();
    }
    write_set->clear();
    if(enable_logging){
        if(use_raft){
            brpc::ChannelOptions options;
            options.timeout_ms = 10000;
            options.max_retry = 3;
            brpc::Channel channel;
            // TODO: node ip need
            IP_Port node;
            if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
            }
            raft_log::Raft_Service_Stub stub(&channel);
            brpc::Controller cntl;
            raft_log::SendRaftLogRequest request;
            raft_log::SendRaftLogResponse response;
            RaftLogRecord raftlog(txn->get_txn_id(), RedoRecordType::ABORT);
            request.set_raft_log(reinterpret_cast<char*>(&raftlog), raftlog.GetSize());
            stub.SendRaftLog(&cntl, &request, &response, NULL);
            if(cntl.Failed()) {
                LOG(WARNING) << cntl.ErrorText();
            }
        }

        //写Abort日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::ABORT);
        auto lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
        // force log
        log_manager_->Flush(lsn, true);
    }
    // Release all the locks.
    ReleaseLocks(txn);
    // Remove txn from txn_map
    std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    txn_map.erase(txn->get_txn_id() );
    // Release the global transaction latch.
    global_txn_latch_.unlock_shared();
    return true;
}

bool TransactionManager::CommitSingle(Transaction * txn, bool sync_write_set, bool use_raft){
    // std::cout << txn->get_txn_id() << " " << "commit" << std::endl;
    if(txn->get_state() == TransactionState::ABORTED){
        return false;
    }
    if(enable_logging){
        if(use_raft){
            brpc::ChannelOptions options;
            options.timeout_ms = 10000;
            options.max_retry = 3;
            brpc::Channel channel;
            // TODO: node ip need
            IP_Port node;
            if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
            }
            raft_log::Raft_Service_Stub stub(&channel);
            brpc::Controller cntl;
            raft_log::SendRaftLogRequest request;
            raft_log::SendRaftLogResponse response;
            if(sync_write_set){
                RaftLogRecord raftlog(txn->get_txn_id(), txn->get_write_set(), false, true);
                request.set_raft_log(reinterpret_cast<char*>(&raftlog), raftlog.GetSize());
            }else{
                RaftLogRecord raftlog(txn->get_txn_id(), RedoRecordType::COMMIT);
                request.set_raft_log(reinterpret_cast<char*>(&raftlog), raftlog.GetSize());
            }
            stub.SendRaftLog(&cntl, &request, &response, NULL);
            if(cntl.Failed()) {
                LOG(WARNING) << cntl.ErrorText();
            }
        }

        //写Commit日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::COMMIT);
        auto lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
        // force log
        log_manager_->Flush(lsn, true);
    }
    // Release all locks
    ReleaseLocks(txn);
    txn->set_transaction_state(TransactionState::COMMITTED);
    // Remove txn from txn_map
    std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    txn_map.erase(txn->get_txn_id() );
    // Release the global transaction latch.
    global_txn_latch_.unlock_shared();
    return true;
}

bool TransactionManager::Abort(Transaction * txn){
    txn->set_transaction_state(TransactionState::ABORTED);
    if(txn->get_is_distributed() == false){
        AbortSingle(txn);
    }
    else{
        brpc::ChannelOptions options;
        options.timeout_ms = 10000;
        options.max_retry = 3;
        // 创建一个存储std::future对象的vector，用于搜集所有匿名函数的返回值
        std::vector<std::future<bool>> futures;

        for(auto node: *txn->get_distributed_node_set()){
            futures.push_back(std::async(std::launch::async, [&txn, node, &options]()->bool{
                brpc::Channel channel;
                if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                    LOG(ERROR) << "Fail to initialize channel";
                    return false;
                }
                transaction_manager::TransactionManagerService_Stub stub(&channel);
                brpc::Controller cntl;
                transaction_manager::AbortRequest request;
                transaction_manager::AbortResponse response;
                request.set_txn_id(txn->get_txn_id());
                stub.AbortTransaction(&cntl, &request, &response, NULL);
                if(cntl.Failed()) {
                    LOG(WARNING) << cntl.ErrorText();
                }
                return response.ok();
            }) );
        }
        for(size_t i=0; i<(*txn->get_distributed_node_set()).size(); i++){
            if(futures[i].get() == false){
                return false;
            }
        }
    }
    return true;
}

bool TransactionManager::Commit(Transaction * txn){
    if(txn->get_is_distributed() == false){
        if((*txn->get_distributed_node_set()->begin()).ip_addr == FLAGS_SERVER_LISTEN_ADDR &&
                (*txn->get_distributed_node_set()->begin()).port == FLAGS_SERVER_LISTEN_PORT ){
            // 本地事务 无需rpc直接可以调用本地函数
            if(CommitSingle(txn, true, true) == true){
                return true;
            }
            else{
                AbortSingle(txn);
                return false;
            }
        }
        else{
            // 非分布式事务，但数据不在协调者节点上
            brpc::ChannelOptions options;
            options.timeout_ms = 10000;
            options.max_retry = 3;
            brpc::Channel channel;

            auto node = *txn->get_distributed_node_set()->begin();
            if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
                return false;
            }
            transaction_manager::TransactionManagerService_Stub stub(&channel);
            brpc::Controller cntl;
            transaction_manager::CommitRequest request;
            transaction_manager::CommitResponse response;
            request.set_txn_id(txn->get_txn_id());
            stub.CommitTransaction(&cntl, &request, &response, NULL);
            if(cntl.Failed()) {
                LOG(WARNING) << cntl.ErrorText();
            }
            if(response.ok()==false){
                cntl.Reset();
                transaction_manager::AbortRequest abort_request;
                transaction_manager::AbortResponse abort_response;
                abort_request.set_txn_id(txn->get_txn_id());
                stub.AbortTransaction(&cntl, &abort_request, &abort_response, NULL);
                if(cntl.Failed()) {
                    LOG(WARNING) << cntl.ErrorText();
                    return false;
                }
                return false;
            }
            return true;
        }
    }
    else {
        //分布式事务, 2PC两阶段提交
        brpc::ChannelOptions options;
        options.timeout_ms = 10000;
        options.max_retry = 3;
        // 创建一个存储std::future对象的vector，用于搜集所有匿名函数的返回值
        std::vector<std::future<bool>> futures_prepare;

        //准备阶段
        for(auto node: *txn->get_distributed_node_set()){
            futures_prepare.push_back(std::async(std::launch::async, [&txn, node, &options]()->bool{
                // LOG(INFO) << "prepare pause: " <<  node.ip_addr << " " << node.port << std::endl;
                brpc::Channel channel;
                if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                    LOG(ERROR) << "Fail to initialize channel";
                    return false;
                }
                transaction_manager::TransactionManagerService_Stub stub(&channel);
                brpc::Controller cntl;
                transaction_manager::PrepareRequest request;
                transaction_manager::PrepareResponse response;
                request.set_txn_id(txn->get_txn_id());
                
                // hcydebugfault
                // 设置协调者监听ip
                // transaction_manager::PrepareRequest_IP_Port t;
                // t.set_ip_addr(FLAGS_SERVER_LISTEN_ADDR);
                // t.set_port(FLAGS_SERVER_LISTEN_PORT);
                // request.set_allocated_coor_ip(&t);

                // // 为了容错, 将事务涉及到的所有节点发送给每一个参与者
                // for(auto x : *txn->get_distributed_node_set() ){
                //     transaction_manager::PrepareRequest_IP_Port* t = request.add_ips();
                //     t->set_ip_addr(x.ip_addr.c_str());
                //     t->set_port(x.port);
                // }
                
                stub.PrepareTransaction(&cntl, &request, &response, NULL);
                if(cntl.Failed()) {
                    LOG(WARNING) << cntl.ErrorText();
                }
                return response.ok();
            }) );
        }
        //检查各个参与者准备结果
        bool commit_flag = true;
        for(size_t i=0; i<(*txn->get_distributed_node_set()).size(); i++){
            if(futures_prepare[i].get() == false){
                commit_flag = false;
                break;
            }
        }
        //TODO: write Coordinator backup
        
        //提交阶段
        if(commit_flag == false){
            //abort all
            Abort(txn);
        }
        else{
            //commit all
            std::vector<std::future<void>> futures_commit;
            for(auto node: *txn->get_distributed_node_set()){
                // LOG(INFO) << "commit pause: " <<  node.ip_addr << " " << node.port << std::endl;
                futures_commit.push_back(std::async(std::launch::async, [&txn, node, &options](){
                    brpc::Channel channel;
                    if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
                        LOG(ERROR) << "Fail to initialize channel";
                        // return false;
                    }
                    transaction_manager::TransactionManagerService_Stub stub(&channel);
                    brpc::Controller cntl;
                    transaction_manager::CommitRequest request;
                    transaction_manager::CommitResponse response;
                    request.set_txn_id(txn->get_txn_id());
                    stub.CommitTransaction(&cntl, &request, &response, NULL);
                    if(cntl.Failed()) {
                        LOG(WARNING) << cntl.ErrorText();
                    }
                    // return response.ok();
                }) );
            }
            for(size_t i=0; i<(*txn->get_distributed_node_set()).size(); i++){
                futures_commit[i].get();
            }
        }
        return commit_flag;
    }
    return true;
}

bool TransactionManager::PrepareCommit(Transaction * txn){
    // std::cout << txn->get_txn_id() << " " << "prepare commit" << std::endl;
    if(txn->get_state() == TransactionState::ABORTED){
        return false;
    }
    if(txn->get_state() == TransactionState::PREPARED){
        return true;
    }
    if(enable_logging){
        //TODO: 此处在写raft log, 并返回同步结果
        RaftLogRecord raftlog(txn->get_txn_id(), txn->get_write_set(), true, false);
        brpc::ChannelOptions options;
        options.timeout_ms = 10000;
        options.max_retry = 3;
        brpc::Channel channel;
        // TODO: node ip need
        IP_Port node;
        if (channel.Init(node.ip_addr.c_str(), node.port , &options) != 0) {
            LOG(ERROR) << "Fail to initialize channel";
        }
        raft_log::Raft_Service_Stub stub(&channel);
        brpc::Controller cntl;
        raft_log::SendRaftLogRequest request;
        raft_log::SendRaftLogResponse response;
        request.set_raft_log(reinterpret_cast<char*>(&raftlog), raftlog.GetSize());
        stub.SendRaftLog(&cntl, &request, &response, NULL);
        if(cntl.Failed()) {
            LOG(WARNING) << cntl.ErrorText();
        }

        //写Prepared日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::PREPARED);
        auto lsn = log_manager_->AppendLogRecord(record);
        if(lsn == INVALID_LSN) return false;
        txn->set_prev_lsn(lsn);
        // force log
        log_manager_->Flush(lsn, true);
    }
    txn->set_transaction_state(TransactionState::PREPARED);
    return true;
}