#include <butil/logging.h>
#include <brpc/server.h>
#include "distributed_plan_service.pb.h"
#include "planner.h"

#define listen_addr "[::0]:8002"
#define idle_timeout_s -1

/*
**这是接收远程执行计划的节点, 从远程发送来的执行计划首先应该从brpc中解析到
**单机可执行的执行计划数据结构之后正常执行
*/

namespace distributed_plan_service{
class RemotePlanNodeImpl : public RemotePlanNode{
public:
    RemotePlanNodeImpl() {};
    virtual ~RemotePlanNodeImpl() {};

    virtual void SendRemotePlan( google::protobuf::RpcController* cntl_base,
                      const RemotePlan* request,
                      ValuePlan* response,
                      google::protobuf::Closure* done);
};

void ConvertIntoPlan(const ChildPlan* child_plan, std::shared_ptr<Operators> operators);

void RemotePlanNodeImpl::SendRemotePlan( google::protobuf::RpcController* cntl_base,
                      const RemotePlan* request,
                      ValuePlan* response,
                      google::protobuf::Closure* done){

        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

        // std::shared_ptr<Operators> operators = std::make_shared<Operators>();
        std::shared_ptr<Operators> operators = nullptr;
        ConvertIntoPlan(&request->child(), operators);
        
}

void ConvertIntoPlan(const ChildPlan* child_plan, std::shared_ptr<Operators> operators){
    auto cur_op = operators;
    while (child_plan->child_plan_case() != ChildPlan::ChildPlanCase::CHILD_PLAN_NOT_SET){
        switch ( child_plan->child_plan_case()){
            case ChildPlan::ChildPlanCase::kSeqScanPlan :{
                auto ptr = std::make_shared<op_tablescan>();
                ptr->db_name = child_plan->seq_scan_plan().db_name();
                ptr->tabs = child_plan->seq_scan_plan().tab_name();
                ptr->par_id = child_plan->seq_scan_plan().par_id();

                cur_op = ptr;
                cur_op = cur_op->next_node;
                child_plan = &child_plan->project_plan().child()[0];
                break;
            }
            case ChildPlan::ChildPlanCase::kFilterPlan :{

                break;
            }
            case ChildPlan::ChildPlanCase::kProjectPlan :{
                auto ptr = std::make_shared<op_projection>();
                std::string tab_name = child_plan->project_plan().tab_name();
                for(auto col : child_plan->project_plan().col_name()){
                    ptr->cols.push_back(std::make_shared<ast::Col>(tab_name,col));
                }
                
                cur_op = ptr;
                cur_op = cur_op->next_node;
                child_plan = &child_plan->project_plan().child()[0];
                break;
            }
            case ChildPlan::ChildPlanCase::kInsertPlan :
                /* code */
                break;
            case ChildPlan::ChildPlanCase::kDeletePlan :
                /* code */
                break;
            case ChildPlan::ChildPlanCase::kUpdatePlan :
                /* code */
                break;
            case ChildPlan::ChildPlanCase::kNestedloopJoinPlan : {
                auto ptr = std::make_shared<op_join>();
                for(int i=0; i<child_plan->nestedloop_join_plan().child_size(); i++){
                    std::shared_ptr<Operators> ptr_child;
                    ConvertIntoPlan(&child_plan->nestedloop_join_plan().child()[i], ptr_child);
                    ptr->tables_get.push_back(ptr_child);
                }
                cur_op = ptr;
                return;
            }
            case ChildPlan::ChildPlanCase::kTableGetPan :
                /* code */
                break;
            case ChildPlan::ChildPlanCase::kValuePlan :
                /* code */
                break;
            default:
                break;
        }
    }
   
}


}