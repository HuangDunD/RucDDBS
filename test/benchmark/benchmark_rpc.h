#include <butil/logging.h> 
#include <brpc/server.h>
#include <gflags/gflags.h>

#include "benchmark_operator.pb.h"
#include "transaction_manager.h"

namespace benchmark_service {
class BenchmarkServiceImpl : public BenchmarkService{
public:
    explicit BenchmarkServiceImpl(TransactionManager *transaction_manager):transaction_manager_(transaction_manager) {};
    virtual void SendOperator(::google::protobuf::RpcController* controller,
                       const ::benchmark_service::OperatorRequest* request,
                       ::benchmark_service::OperatorResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                uint64_t txn_id = request->txn_id();
                std::shared_lock<std::shared_mutex> l(transaction_manager_->txn_map_mutex);
                Transaction *txn =nullptr;
                if(transaction_manager_->txn_map.count(txn_id) != 0){
                    txn = transaction_manager_->txn_map[txn_id];
                }else{
                    l.unlock();
                    transaction_manager_->Begin(txn, txn_id);
                }

                if(request->op_type() == OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Get){
                    std::string key = request->key();
                    int row_id = std::stoi(key.substr(3, key.length()-3));
                    transaction_manager_->getLockManager()->LockRow(txn, LockMode::SHARED, 0, 0, row_id);
                    std::string value = transaction_manager_->getKVstore()->get(key).second;
                }
                else if(request->op_type() == OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Put){
                    std::string key = request->key();
                    std::string value = request->value();
                    int row_id = std::stoi(key.substr(3, key.length()-3));
                    transaction_manager_->getLockManager()->LockRow(txn, LockMode::EXLUCSIVE, 0, 0, row_id);
                    transaction_manager_->getKVstore()->put(key, value, txn);
                }
                else if(request->op_type() == OperatorRequest_OP_TYPE::OperatorRequest_OP_TYPE_Del){
                    std::string key = request->key();
                    int row_id = std::stoi(key.substr(3, key.length()-3));
                    transaction_manager_->getLockManager()->LockRow(txn, LockMode::EXLUCSIVE, 0, 0, row_id);
                    transaction_manager_->getKVstore()->del(key, txn);
                }

                response->set_ok(true);
                return;
    }
private:
    TransactionManager *transaction_manager_;
};
}

