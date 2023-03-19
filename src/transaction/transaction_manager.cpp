#include "transaction_manager.h"

std::atomic<bool> enable_logging(true);

//-------------------------------------
// TODO 与存储层的接口
class KV{
public:
    void RollbackDelete(char* key, int32_t key_size, char* value, int32_t value_size, Transaction* txn){};
    void ApplyDelete(char* key, int32_t key_size, char* value, int32_t value_size, Transaction* txn){};
    void UpdateKV(char* key, int32_t key_size, char* old_value, int32_t old_value_size, Transaction* txn){};

};

KV kv;

//-------------------------------------

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};
std::shared_mutex TransactionManager::txn_map_mutex = {};
   
uint64_t TransactionManager::getTimestampFromServer(){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    if (channel.Init(server, &options) != 0) {
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

Transaction* TransactionManager::Begin(Transaction* txn, IsolationLevel isolation_level=IsolationLevel::SERIALIZABLE){
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
        //TODO 注意此处可能存在record生命周期结束析构的情况
        lsn_t lsn = log_manager_->AppendLogRecord(&record);
        txn->set_prev_lsn(lsn);
    }

    std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    txn_map[txn->get_txn_id()] = txn;
    return txn;
}

void TransactionManager::Abort(Transaction * txn){
    txn->set_transaction_state(TransactionState::ABORTED);
    if(txn->get_is_distributed() == false){
        auto write_set = txn->get_write_set();
        while (!write_set->empty()) {
            auto &item = write_set->back();
            if(item.getWType() == WType::DELETE_TUPLE){
                kv.RollbackDelete(item.getKey(), item.getKeySize(), item.getValue(), item.getValueSize(), txn);
            }else if(item.getWType() == WType::INSERT_TUPLE){
                kv.ApplyDelete(item.getKey(), item.getKeySize(), item.getValue(), item.getValueSize(), txn);
            }else if (item.getWType() == WType::UPDATE_TUPLE){
                kv.UpdateKV(item.getKey(), item.getKeySize(), item.getOldValue(), item.getOldValueSize(), txn);
            }
            write_set->pop_back();
        }
        write_set->clear();
        //写Abort日志
        LogRecord* record = new LogRecord(txn->get_txn_id(),
            txn->get_prev_lsn(), LogRecordType::ABORT);
        log_manager_->AppendLogRecord(record);

        // Release all the locks.
        ReleaseLocks(txn);
        // Release the global transaction latch.
        global_txn_latch_.unlock_shared();
    }
    else{
        auto node_set = txn->get_distributed_node_set();


    }
}

void TransactionManager::Commit(Transaction * txn){
    
}

int main(){
    return 0;
}