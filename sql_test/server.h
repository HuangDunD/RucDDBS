#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "session.pb.h"
#include "engine/engine.h"

DEFINE_int32(port, 8002, "TCP Port of this server");
DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");


namespace session{
void ConvertIntoPlan(const ChildPlan* child_plan, std::shared_ptr<Operators> operators){
    auto cur_op = operators;
    if (child_plan->child_plan_case() != ChildPlan::ChildPlanCase::CHILD_PLAN_NOT_SET){
        switch ( child_plan->child_plan_case()){
            case ChildPlan::ChildPlanCase::kSeqScanPlan :{
                
                auto ptr = std::make_shared<op_tablescan>();
                ptr->tabs = child_plan->seq_scan_plan().tab_name();
                ptr->par = child_plan->seq_scan_plan().par_id();
    
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                
                child_plan = &(child_plan->seq_scan_plan().child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kFilterPlan: {
                auto ptr = std::make_shared<op_selection>();

                // 将过滤条件转换为表达式树
                const auto& filter_plan = child_plan->filter_plan();
                
                auto exp = filter_plan.conds();
                ptr->conds->lhs = make_shared<ast::Col>();
                ptr->conds->lhs->tab_name = exp.col().tab_name();
                ptr->conds->lhs->col_name = exp.col().col_name();

                ptr->conds->op = ast::SvCompOp(exp.op());
                
                switch(exp.exp().exp_case()){
                    case Expr::ExpCase::kCol: {
                        auto tmp =  make_shared<ast::Col>();
                        ptr->conds->rhs = tmp;
                        tmp->col_name = exp.exp().col().col_name();
                        tmp->col_name = exp.exp().col().tab_name();
                    }
                    case Expr::ExpCase::kVal:{
                        auto val_ = exp.exp().val();
                        // 根基val_构造
                        shared_ptr<ast::IntLit> val(new ast::IntLit(val_.value()));
                        ptr->conds->rhs = val;
                    }
                }

                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;

                child_plan = &(filter_plan.child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kInsertPlan :{
                /* code */
                auto ptr = std::make_shared<op_insert>();
                ptr->tab_name = child_plan->insert_plan().tab_name();
                ptr->par = child_plan->insert_plan().par_id();

                for(int i = 0; i < child_plan->insert_plan().col_name().size(); i++){
                    shared_ptr<value> val_(new value);
                    val_->col_name = child_plan->insert_plan().col_name()[i];
                    val_->str = child_plan->insert_plan().val()[i];
                    val_->tab_name = ptr->tab_name;
                    ptr->rec->row.push_back(val_);
                }

                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                child_plan = &(child_plan->insert_plan().child()[0]);
                break;
            }    
            case ChildPlan::ChildPlanCase::kDeletePlan: {
                auto ptr = std::make_shared<op_delete>();
                ptr->tab_name = child_plan->delete_plan().tab_name();
                ptr->par = child_plan->delete_plan().par_id();
                cur_op->next_node = ptr;
                cur_op = cur_op->next_node;
                child_plan = &(child_plan->delete_plan().child()[0]);
                break;
            }
            case ChildPlan::ChildPlanCase::kUpdatePlan: {
                auto ptr = std::make_shared<op_update>();
                ptr->tab_name = child_plan->update_plan().tab_name();
                ptr->par = child_plan->update_plan().par_id();
                
                int size = child_plan->update_plan().set_col_index().size();
                for(int i = 0; i < size ; i++){
                    ptr->set_col_index.push_back(child_plan->update_plan().set_col_index()[i]);
                    shared_ptr<value> val_(new value);
                    val_->str = val_->str;
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
    Session_Service_Impl() {}
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

        cout << request->sql_statement() << endl;
        auto sql_ret = Sql_execute(request->sql_statement());
        shared_ptr<Table> response_table(new Table);
        for(auto row: sql_ret){
            shared_ptr<Tuple> response_tuple(response_table->add_table());
            for(auto var: row->row){
                shared_ptr<Value> response_value(response_tuple->add_tuple());
                response_value->set_value(var->str);
            }
        }
        response->set_allocated_sql_response(response_table.get());
    }
    virtual void SendRemotePlan(google::protobuf::RpcController* cntl_base,
                      const RemotePlan* request,
                      Table* response,
                      google::protobuf::Closure* done){
        
        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        std::shared_ptr<op_executor> operators(new op_executor);
        
        ConvertIntoPlan(&request->child(),operators);
        cout << "chech_point2" << endl;
        operators->prt_op();
        auto res = operators->exec_op();
        cout << "chech_point3" << endl;
        for(auto row: res){
            shared_ptr<Tuple> response_tuple(response->add_table());
            for(auto var: row->row){
                shared_ptr<Value> response_value(response_tuple->add_tuple());
                response_value->set_value(var->str);
            }
        }
    }
};
}