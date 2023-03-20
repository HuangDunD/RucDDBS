#include <butil/logging.h> 
#include <brpc/server.h>
#include <gflags/gflags.h>

#include "transaction_manager.pb.h"
#include "transaction_manager.h"

DEFINE_int32(port, 8001, "TCP Port of this server");
DEFINE_string(listen_addr, "[::0]:8001", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");

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
                if(!transaction_manager_->Abort(txn)){
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
                if(!transaction_manager_->Commit(txn)){
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