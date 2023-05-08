// transaction_manager_rpc.h
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
                // TODO: 从request中获取txn_id, 调用transaction_manager_的abort函数
                // 并设置response的rpc返回值

                return;
        }

    virtual void PrepareTransaction(::google::protobuf::RpcController* controller,
                       const ::transaction_manager::PrepareRequest* request,
                       ::transaction_manager::PrepareResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                
                return;
        }

    virtual void CommitTransaction(::google::protobuf::RpcController* controller,
                       const ::transaction_manager::CommitRequest* request,
                       ::transaction_manager::CommitResponse* response,
                       ::google::protobuf::Closure* done){

                brpc::ClosureGuard done_guard(done);
                
                return;
       }

private:
    TransactionManager *transaction_manager_;
 };
} 