#include <brpc/channel.h>
#include <butil/logging.h>
#include <future>
#include <functional>

#include "transaction_manager.h"
#include "transaction_manager.pb.h"
#include "dbconfig.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};
std::shared_mutex TransactionManager::txn_map_mutex = {};
   
uint64_t TransactionManager::getTimestampFromServer(){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
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
    
    for(const auto &lock_row: *txn->get_row_S_lock_set()){
        lock_manager_->UnLockRow(txn, lock_row.oid_, lock_row.p_id_, lock_row.row_id_);
    }
    for(const auto &lock_row: *txn->get_row_X_lock_set()){
        lock_manager_->UnLockRow(txn, lock_row.oid_, lock_row.p_id_, lock_row.row_id_);
    }

    for(const auto &lock_par: *txn->get_partition_S_lock_set()){
        lock_manager_->UnLockPartition(txn, lock_par.oid_, lock_par.p_id_);
    }
    for(const auto &lock_par: *txn->get_partition_IS_lock_set()){
        lock_manager_->UnLockPartition(txn, lock_par.oid_, lock_par.p_id_);
    }
    for(const auto &lock_par: *txn->get_partition_IX_lock_set()){
        lock_manager_->UnLockPartition(txn, lock_par.oid_, lock_par.p_id_);
    }
    for(const auto &lock_par: *txn->get_partition_SIX_lock_set()){
        lock_manager_->UnLockPartition(txn, lock_par.oid_, lock_par.p_id_);
    }
    for(const auto &lock_par: *txn->get_partition_X_lock_set()){
        lock_manager_->UnLockPartition(txn, lock_par.oid_, lock_par.p_id_);
    }

    for(const auto &lock_tab: *txn->get_table_S_lock_set()){
        lock_manager_->UnLockTable(txn, lock_tab.oid_);
    }
    for(const auto &lock_tab: *txn->get_table_IS_lock_set()){
        lock_manager_->UnLockTable(txn, lock_tab.oid_);
    }
    for(const auto &lock_tab: *txn->get_table_SIX_lock_set()){
        lock_manager_->UnLockTable(txn, lock_tab.oid_);
    }
    for(const auto &lock_tab: *txn->get_table_IX_lock_set()){
        lock_manager_->UnLockTable(txn, lock_tab.oid_);
    }
    for(const auto &lock_tab: *txn->get_table_X_lock_set()){
        lock_manager_->UnLockTable(txn, lock_tab.oid_);
    }
    
    return ;
}

Transaction* TransactionManager::Begin(Transaction*& txn, IsolationLevel isolation_level){
    // Todo:
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

bool TransactionManager::AbortSingle(Transaction * txn){
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        auto &item = write_set->back();
        if(item.getWType() == WType::DELETE_TUPLE){
            kv_->put(std::string(item.getKey(),item.getKeySize()), std::string(item.getValue(),item.getValueSize()), txn);
        }else if(item.getWType() == WType::INSERT_TUPLE){
            kv_->del(std::string(item.getKey(), item.getKeySize()),txn);
        }else if (item.getWType() == WType::UPDATE_TUPLE){
            kv_->del(std::string(item.getKey(),item.getKeySize()), txn);
            kv_->put(std::string(item.getKey(), item.getKeySize()), std::string(item.getOldValue(), item.getOldValueSize()), txn);
        }
        write_set->pop_back();
    }
    write_set->clear();
    if(enable_logging){
        //写Abort日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::ABORT);
        auto lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
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

bool TransactionManager::CommitSingle(Transaction * txn){
    if(txn->get_state() == TransactionState::ABORTED){
        return false;
    }
    txn->set_transaction_state(TransactionState::COMMITTED);
    if(enable_logging){
        //写Commit日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::COMMIT);
        auto lsn = log_manager_->AppendLogRecord(record);
        txn->set_prev_lsn(lsn);
    }

    // Release all locks
    ReleaseLocks(txn);
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
        options.timeout_ms = 100;
        options.max_retry = 3;
        // 创建一个存储std::future对象的vector，用于搜集所有匿名函数的返回值
        std::vector<std::future<bool>> futures;

        for(auto node: *txn->get_distributed_node_set()){
            futures.push_back(std::async(std::launch::async, [&txn, &node, &options]()->bool{
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
        return CommitSingle(txn);
    }
    else {
        //分布式事务, 2PC两阶段提交
        brpc::ChannelOptions options;
        options.timeout_ms = 100;
        options.max_retry = 3;
        // 创建一个存储std::future对象的vector，用于搜集所有匿名函数的返回值
        std::vector<std::future<bool>> futures_prepare;

        //准备阶段
        for(auto node: *txn->get_distributed_node_set()){
            futures_prepare.push_back(std::async(std::launch::async, [&txn, &node, &options]()->bool{
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
                futures_commit.push_back(std::async(std::launch::async, [&txn, &node, &options](){
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
        }
    }
    return true;
}

bool TransactionManager::PrepareCommit(Transaction * txn){
    if(txn->get_state() == TransactionState::ABORTED){
        return false;
    }
    if(enable_logging){
        //写Prepared日志
        LogRecord record(txn->get_txn_id(), txn->get_prev_lsn(), LogRecordType::PREPARED);
        //TODO: 此处在kv层写redo/undo log, raft返回同步结果

        auto lsn = log_manager_->AppendLogRecord(record);
        if(lsn == INVALID_LSN) return false;
        txn->set_prev_lsn(lsn);
    }
    txn->set_transaction_state(TransactionState::PREPARED);
    return true;
}