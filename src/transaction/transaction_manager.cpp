#include "transaction_manager.h"

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

Transaction* TransactionManager::Begin(Transaction* txn, LogManager *log_manager){
    // Todo:
    // 1. 判断传入事务参数是否为空指针
    // 2. 如果为空指针，创建新事务
    // 3. 把开始事务加入到全局事务表中
    // 4. 返回当前事务指针
    global_txn_latch_.lock_shared();

    if (txn == nullptr) {
        txn = new Transaction(getTimestampFromServer(), IsolationLevel::SERIALIZABLE); 
    }

    //TODO logging


    std::unique_lock<std::shared_mutex> l(txn_map_mutex);
    txn_map[txn->get_txn_id()] = txn;
    return txn;
}

void TransactionManager::Abort(Transaction * txn, LogManager *log_manager){

}

void TransactionManager::Commit(Transaction * txn, LogManager *log_manager){
    
}
