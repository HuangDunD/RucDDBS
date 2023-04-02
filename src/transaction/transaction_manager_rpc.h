#include <butil/logging.h> 
#include <brpc/server.h>
#include <gflags/gflags.h>

#include "transaction_manager.pb.h"
#include "transaction_manager.h"

namespace transaction_manager {
class TransactionManagerImpl : public TransactionManagerService{
public:
    TransactionManagerImpl(TransactionManager *transaction_manager):transaction_manager_(transaction_manager) {};
    TransactionManagerImpl() {};
    virtual ~TransactionManagerImpl() {};

    virtual void AbortTransaction(::google::protobuf::RpcController* controller,
                       const ::transaction_manager::AbortRequest* request,
                       ::transaction_manager::AbortResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                txn_id_t txn_id = request->txn_id();
                auto txn = transaction_manager_->getTransaction(txn_id);
                if(txn == nullptr){
                    response->set_ok(false);
                    return;
                }
                if(!transaction_manager_->AbortSingle(txn)){
                    response->set_ok(false); 
                    return;
                }
                response->set_ok(true);
                return;
        }

    virtual void PrepareTransaction(::google::protobuf::RpcController* controller,
                       const ::transaction_manager::PrepareRequest* request,
                       ::transaction_manager::PrepareResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                txn_id_t txn_id = request->txn_id();
                auto txn = transaction_manager_->getTransaction(txn_id);
                if(txn == nullptr){
                    response->set_ok(false);
                    return;
                }
                if(!transaction_manager_->PrepareCommit(txn)){
                    response->set_ok(false);
                    return;
                }
                response->set_ok(true);
                return;
        }

    virtual void CommitTransaction(::google::protobuf::RpcController* controller,
                       const ::transaction_manager::CommitRequest* request,
                       ::transaction_manager::CommitResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                txn_id_t txn_id = request->txn_id();
                auto txn = transaction_manager_->getTransaction(txn_id);
                if(txn == nullptr){
                    response->set_ok(false);
                    return;
                }
                if(!transaction_manager_->CommitSingle(txn)){
                    response->set_ok(false);
                    return;
                }
                response->set_ok(true);
                return;
       }

private:
    TransactionManager *transaction_manager_;
 };
} 