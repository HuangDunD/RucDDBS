#pragma once
#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "session.pb.h"
#include "engine.h"
#include "kv_store.h"
#include "transaction_manager.h"

namespace session{
void ConvertIntoPlan(const ChildPlan* child_plan, std::shared_ptr<Operators> operators, Context *context){
    auto cur_op = operators;
    while (child_plan->child_plan_case() != ChildPlan::ChildPlanCase::CHILD_PLAN_NOT_SET){
        cout << "get - plan" << endl;
        switch ( child_plan->child_plan_case()){
            case ChildPlan::ChildPlanCase::kSeqScanPlan :{
                cout << "kSeqScanPlan " << endl;
                auto ptr = std::make_shared<op_tablescan>(context);
                ptr->tabs = child_plan->seq_scan_plan().tab_name();
                ptr->par = child_plan->seq_scan_plan().par_id();
    
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                
                child_plan = &(child_plan->seq_scan_plan().child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kFilterPlan: {
                cout << "kFilterPlan " << endl;
                auto ptr = std::make_shared<op_selection>(context);

                // 将过滤条件转换为表达式树
                const auto& filter_plan = child_plan->filter_plan();
                auto exp = filter_plan.conds();
                auto lhs = make_shared<ast::Col>(exp.col().tab_name(), exp.col().col_name());
                auto op = ast::SvCompOp(exp.op());
                
                switch( (exp.exp().exp_case())){
                    case Expr::ExpCase::kCol: {
                        auto tmp =  make_shared<ast::Col>(exp.exp().col().col_name(), exp.exp().col().tab_name());
                        ptr->conds = make_shared<ast::BinaryExpr>(lhs, op, tmp);
                    }
                    case Expr::ExpCase::kVal:{
                        auto val_ = exp.exp().val();
                        // 根基val_构造
                        auto val = make_shared<ast::IntLit>(val_.value());
                        ptr->conds = make_shared<ast::BinaryExpr>(lhs, op, val);
                    }
                    default:
                        break;
                }
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;

                child_plan = &(filter_plan.child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kInsertPlan :{
                /* code */
                cout << "KInsert Plan" << endl;
                auto ptr = std::make_shared<op_insert>(context);
                ptr->tab_name = child_plan->insert_plan().tab_name();
                ptr->par = child_plan->insert_plan().par_id();
                
                for(int i = 0; i < child_plan->insert_plan().col_name().size(); i++){
                    shared_ptr<value> val_(new value);
                    val_->col_name = child_plan->insert_plan().col_name(i);
                    val_->str = child_plan->insert_plan().val(i);
                    val_->tab_name = ptr->tab_name;
                    ptr->rec->row.push_back(val_);
                }
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                
                child_plan = &(child_plan->insert_plan().child()[0]);
                break;
            }    
            case ChildPlan::ChildPlanCase::kDeletePlan: {
                cout << "kDeletePlan " << endl;
                auto ptr = std::make_shared<op_delete>(context);
                ptr->tab_name = child_plan->delete_plan().tab_name();
                ptr->par = child_plan->delete_plan().par_id();
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                child_plan = &(child_plan->delete_plan().child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kUpdatePlan: {
                cout << "kUpdatePlan " << endl;
                auto ptr = std::make_shared<op_update>(context);
                ptr->tab_name = child_plan->update_plan().tab_name();
                ptr->par = child_plan->update_plan().par_id();
                
                int size = child_plan->update_plan().set_col_index().size();
                for(int i = 0; i < size ; i++){
                    ptr->set_col_index.push_back(child_plan->update_plan().set_col_index()[i]);
                    auto val_ = make_shared<value>();
                    val_->str = child_plan->update_plan().set_val()[i];
                    ptr->set_val.push_back(val_);
                }

                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                child_plan = &(child_plan->update_plan().child()[0]);
                break;
            }
            default:
                break;
        }
    }
}

class Session_Service_Impl : public Session_Service{
public:
    Session_Service_Impl(TransactionManager* transaction_manager_sql) { transaction_manager_ = transaction_manager_sql;}
    virtual ~Session_Service_Impl() {}
    virtual void SQL_Transfer(google::protobuf::RpcController* cntl_base,
                      const SQL_Request* request,
                      SQL_Response* response,
                      google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);

        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        
        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
                  << "] from " << cntl->remote_side() 
                  << " to " << cntl->local_side()
                  << ": " << request->sql_statement();

        cout << request->sql_statement() << "id = "<< request->txn_id() <<endl;
        txn_id_t txn_id = request->txn_id();
        Context* context = new Context(nullptr, transaction_manager_);
        try {
            auto sql_ret = Sql_execute_client(request->sql_statement(), txn_id, context);
            cout << "sql - over" << endl;
            cout << sql_ret << endl;
            response->set_txt(sql_ret);
            response->set_txn_id(txn_id);
        }
        catch(TransactionAbortException &e)
        {
            context->txn_->set_transaction_state(TransactionState::ABORTED);
            std::cerr << e.GetInfo() << '\n';
            
            context->transaction_manager_->Abort(context->txn_);
            response->set_txt(e.GetInfo());
            response->set_txn_id(0);
        }
        delete context;
    }
    virtual void SendRemotePlan(google::protobuf::RpcController* cntl_base,
                      const RemotePlan* request,
                      Table* response,
                      google::protobuf::Closure* done){
        
        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
                  << "] from " << cntl->remote_side() 
                  << " to " << cntl->local_side();
        
        uint64_t txn_id = request->txn_id();
        Transaction* txn = transaction_manager_->getTransaction(txn_id);
        if(txn == nullptr) {
            std::unique_lock<std::shared_mutex> l(transaction_manager_->txn_map_mutex);
            transaction_manager_->Begin(txn, txn_id);
        }
        Context *context_ = new Context(txn, transaction_manager_);
        std::shared_ptr<op_executor> operators(new op_executor(context_));
        try
        {
            ConvertIntoPlan(&request->child(),operators, context_);
            auto res = operators->exec_op();
            for(auto row: res){
                cout << row->rec_to_string() << endl;
                auto response_tuple = response->add_table();
                for(auto var: row->row){
                    auto response_value = response_tuple->add_tuple();
                    response_value->set_value(var->str);
                    response_value->set_col_name(var->col_name);
                    response_value->set_tab_name(var->tab_name);
                }
            }
            response->set_abort(false);
        }
        catch(TransactionAbortException &e)
        {
            response->set_abort(true);
            std::cerr << e.GetInfo() << '\n';
        }
        delete context_;
    }
public:
    TransactionManager* transaction_manager_;
};
}