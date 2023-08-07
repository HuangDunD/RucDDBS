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
                if(txn->get_state() == TransactionState::PREPARED){
                    transaction_manager_->AbortSingle(txn, true);
                }
                else if(!transaction_manager_->AbortSingle(txn, false)){
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
                // fault_tolerance: 设置参与者节点上的协调者ip
                // txn->set_coor_ip({request->coor_ip().ip_addr(), request->coor_ip().port()});
                // fault_tolerance: 设置参与者节点上的所有参与者节点
                // for(int i=0; i<request->ips_size(); i++){
                //     IP_Port t{request->ips()[i].ip_addr(), request->ips()[i].port()};
                //     txn->get_distributed_node_set()->push_back(t);
                // }
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
            if(txn->get_state() == TransactionState::PREPARED){
                transaction_manager_->CommitSingle(txn, false, true);
            }
            else{
                //直接提交的rpc
                if(!transaction_manager_->CommitSingle(txn, true, true)){
                    response->set_ok(false);
                    return;
                };
            }
            response->set_ok(true);
            return;
       }

private:
    TransactionManager *transaction_manager_;
 };
} 